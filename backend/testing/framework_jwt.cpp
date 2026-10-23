import std;
import cnetmod.core;
import cnetmod.coro;
import cnetmod.executor;
import cnetmod.io;
import cnetmod.security.jwt;


//







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