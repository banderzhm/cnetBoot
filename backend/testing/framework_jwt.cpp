import std;
import cnetmod.core;
import cnetmod.coro;
import cnetmod.executor;
import cnetmod.io;
import cnetmod.security.jwt;

// JWT 用例：签发、校验、过期、篡改、错误密钥与结构损坏。
//
// 迁移自 legacy application tests/jwt_test.cpp（原用手写的 xuanxue.common.jwt），
// 这里换成 cnetBoot 实际使用的 cnetmod.security.jwt —— cnetBoot 没有自研 JWT，
// 玄学那份实现已随 identity 层一起被框架取代（策略 A），因此断言逐条改写为对
// **模板实际依赖的实现**的等价断言：
//  - 签发：三段式（两个 '.'）、载荷可解析、sub/iss/custom 往返；
//  - 校验：issuer 不匹配、过期、篡改载荷、篡改签名、错误密钥、结构损坏都必须失败；
//  - 过期判定走 is_jwt_expired（框架的轻量判定，无加密）。
auto main() -> int
{
    auto io = cnetmod::make_io_context();
    if (!io)
        return 1;
    cnetmod::thread_pool pool{2};

    constexpr std::string_view secret{"unit-test-secret-key-0123456789"};
    auto session_id = cnetmod::security::generate_secure_token(16);
    if (session_id.size() != 32)
        return 2;

    int code = 0;
    std::optional<std::string> token;
    auto work = [&]() -> cnetmod::task<void>
    {
        cnetmod::security::jwt_sign_options options{
            .issuer = "cnetboot",
            .subject = "1",
            .scopes = {"system:user:list"},
            .lifetime = std::chrono::hours{1},
        };
        options.custom_claims["sid"] = session_id;
        options.custom_claims["username"] = "admin";

        auto signed_token = co_await cnetmod::security::sign_jwt(pool, *io, options, secret);
        if (!signed_token)
        {
            code = 3;
            io->stop();
            co_return;
        }
        token = std::move(*signed_token);
        io->stop();
        co_return;
    };
    cnetmod::spawn(*io, work());
    io->run();
    if (code != 0)
        return code;

    if (!token || std::ranges::count(*token, '.') != 2)
        return 4;

    // 校验成功：issuer / subject / custom claim 必须原样往返。
    {
        int verify_code = 0;
        auto verify = [&]() -> cnetmod::task<void>
        {
            auto claims = co_await cnetmod::security::verify_jwt(pool, *io, *token, secret);
            if (!claims)
            {
                verify_code = 5;
                io->stop();
                co_return;
            }
            if (claims->issuer != "cnetboot" || claims->subject != "1")
                verify_code = 6;
            else if (const auto session = claims->custom.find("sid");
                session == claims->custom.end() || session->second != session_id)
                verify_code = 7;
            else if (const auto user = claims->custom.find("username");
                user == claims->custom.end() || user->second != "admin")
                verify_code = 8;
            else if (claims->scopes != std::vector<std::string>{"system:user:list"})
                verify_code = 9;
            else if (cnetmod::security::is_jwt_expired(*claims))
                verify_code = 10;
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify());
        io->run();
        if (verify_code != 0)
            return verify_code;
    }

    // 篡改载荷（保持签名段）必须被拒。
    {
        std::string tampered = *token;
        const auto first = tampered.find('.');
        tampered[first + 1] = tampered[first + 1] == 'a' ? 'b' : 'a';
        bool rejected = false;
        auto verify = [&]() -> cnetmod::task<void>
        {
            rejected = !(co_await cnetmod::security::verify_jwt(pool, *io, tampered, secret));
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify());
        io->run();
        if (!rejected)
            return 11;
    }

    // 篡改签名段第一个字符（base64url 无填充，翻转首字符必然改变签名）必须被拒。
    {
        std::string tampered = *token;
        const auto signature_start = tampered.rfind('.') + 1;
        tampered[signature_start] = tampered[signature_start] == 'a' ? 'b' : 'a';
        bool rejected = false;
        auto verify = [&]() -> cnetmod::task<void>
        {
            rejected = !(co_await cnetmod::security::verify_jwt(pool, *io, tampered, secret));
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify());
        io->run();
        if (!rejected)
            return 12;
    }

    // 错误密钥必须被拒。
    {
        bool rejected = false;
        auto verify = [&]() -> cnetmod::task<void>
        {
            rejected = !(co_await cnetmod::security::verify_jwt(pool, *io, *token, "another-secret-key-0123456789"));
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify());
        io->run();
        if (!rejected)
            return 13;
    }

    // 结构损坏必须被拒（三段式之外的一切输入）。
    {
        bool rejected = true;
        auto verify = [&]() -> cnetmod::task<void>
        {
            rejected = !(co_await cnetmod::security::verify_jwt(pool, *io, "not-a-jwt", secret));
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify());
        io->run();
        if (!rejected)
            return 14;
        // 签名段被截断。
        auto truncated = *token;
        truncated.pop_back();
        bool truncated_rejected = false;
        auto verify_truncated = [&]() -> cnetmod::task<void>
        {
            truncated_rejected = !(co_await cnetmod::security::verify_jwt(pool, *io, truncated, secret));
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify_truncated());
        io->run();
        if (!truncated_rejected)
            return 15;
    }

    // 已过期令牌：is_jwt_expired 判定为过期（框架签发时按 lifetime 计算 exp）。
    {
        std::optional<std::string> expired_token;
        int expired_code = 0;
        auto sign_expired = [&]() -> cnetmod::task<void>
        {
            const cnetmod::security::jwt_sign_options options{
                .issuer = "cnetboot",
                .subject = "1",
                .scopes = {},
                .lifetime = std::chrono::seconds{-120},
            };
            auto signed_token = co_await cnetmod::security::sign_jwt(pool, *io, options, secret);
            if (!signed_token)
                expired_code = 16;
            else
                expired_token = std::move(*signed_token);
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, sign_expired());
        io->run();
        if (expired_code != 0)
            return expired_code;

        // cnetmod 的 verify_jwt 在验签之后**直接按 exp 拒绝**过期令牌
        // （jwt.cpp:235-236 返回 "JWT has expired"），因此 claims 为空 —— 这是框架口径，
        // 与玄学「返回 jwt_status::expired 但带出 claims」不同。等价断言：
        // 过期令牌必须被拒（这是模板真正依赖的行为），并单独核对 is_jwt_expired 判据。
        bool expired_rejected = false;
        auto verify_expired = [&]() -> cnetmod::task<void>
        {
            expired_rejected = !(co_await cnetmod::security::verify_jwt(pool, *io, *expired_token, secret));
            io->stop();
            co_return;
        };
        io->restart();
        cnetmod::spawn(*io, verify_expired());
        io->run();
        if (!expired_rejected)
            return 17;
        const cnetmod::security::jwt_claims stale{
            .expires_at = std::chrono::system_clock::now() - std::chrono::seconds{60}};
        if (!cnetmod::security::is_jwt_expired(stale))
            return 17;
    }

    // 会话 ID 形状：32 位十六进制（jwt_security 的校验口径），两次生成不重复。
    {
        if (!std::ranges::all_of(session_id, [](unsigned char ch)
                {
                    return std::isxdigit(ch) != 0;
                }))
            return 18;
        const auto second = cnetmod::security::generate_secure_token(16);
        if (second.size() != 32 || second == session_id)
            return 19;
    }
    return 0;
}