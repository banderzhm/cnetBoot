module cnetnexus.framework.security.jwt_security;

import std;
import cnetmod.application;
import cnetmod.core.log;
import cnetmod.coro.task;
import cnetmod.json;
import cnetmod.protocol.http;
import cnetmod.protocol.http.middleware.jwt_auth;
import cnetmod.protocol.redis;
import cnetmod.orm;
import cnetmod.security.jwt;
import cnetmod.utils.concurrent_containers.striped_hash_map;
import cnetnexus.framework.error;
import cnetnexus.framework.security.current_user;
import cnetnexus.framework.web.error_response;

namespace cnetnexus::security {
namespace {

    using error_code = cnetnexus::framework::error::error_code;

    /// 会话 ID：128 位随机数的十六进制表示。
    constexpr std::size_t session_id_bytes{16};
    constexpr std::string_view rotate_session_script =
        "if redis.call('EXISTS',KEYS[1])==0 then return 0 end;"
        "redis.call('SET',KEYS[2],ARGV[1],'EX',ARGV[2]);"
        "redis.call('DEL',KEYS[1]);return 1";

    [[nodiscard]] auto parse_user_id(std::string_view subject)
        -> std::optional<std::int64_t>
    {
        std::int64_t result{};
        const auto parsed = std::from_chars(
            subject.data(), subject.data() + subject.size(), result);
        if (parsed.ec != std::errc{} ||
            parsed.ptr != subject.data() + subject.size() || result <= 0)
            return std::nullopt;
        return result;
    }

    [[nodiscard]] auto well_formed_session_id(std::string_view value) -> bool
    {
        return value.size() == session_id_bytes * 2 &&
            std::ranges::all_of(value,
                [](unsigned char character) { return std::isxdigit(character) != 0; });
    }

    /**
     * @brief 本次认证失败对应的注册错误码，经请求作用域交给 on_failure。
     *
     * jwt_auth_failure 只携带通用原因；账号停用这类业务语义由认证器在这里给出
     * 精确的错误码。
     */
    struct authentication_failure_code
    {
        error_code code;
    };

    [[nodiscard]] auto code_for(cnetmod::jwt_auth_reason reason) noexcept -> error_code
    {
        switch (reason)
        {
        case cnetmod::jwt_auth_reason::missing_credentials:
            return error_code::auth_token_missing;
        case cnetmod::jwt_auth_reason::expired_credentials:
            return error_code::auth_token_expired;
        case cnetmod::jwt_auth_reason::unavailable:
            return error_code::upstream_unavailable;
        case cnetmod::jwt_auth_reason::malformed_credentials:
        case cnetmod::jwt_auth_reason::invalid_credentials:
            break;
        }
        return error_code::auth_token_invalid;
    }

    [[nodiscard]] auto reason_for(error_code code) noexcept -> cnetmod::jwt_auth_reason
    {
        switch (code)
        {
        case error_code::upstream_unavailable:
            return cnetmod::jwt_auth_reason::unavailable;
        case error_code::auth_token_expired:
            return cnetmod::jwt_auth_reason::expired_credentials;
        default:
            return cnetmod::jwt_auth_reason::invalid_credentials;
        }
    }

    /// @brief 构造一次失败：错误码写进请求作用域，状态码取注册表。
    [[nodiscard]] auto reject(cnetmod::http::request_context& request, error_code code)
        -> cnetmod::jwt_auth_failure
    {
        request.scope().bind(std::make_shared<authentication_failure_code>(
            authentication_failure_code{.code = code}));
        const auto entry = cnetnexus::framework::error::find(code);
        return cnetmod::jwt_auth_failure{
            .status = entry ? entry->http_status : cnetmod::http::status::unauthorized,
            .message = entry ? std::string{entry->name} : std::string{"AUTH.TOKEN_INVALID"},
            .reason = reason_for(code),
        };
    }

    /**
     * @brief 授权快照的进程内缓存（多事件循环共享，按用户分片加锁）。
     *
     * 会话是否有效每个请求都查 Redis，缓存的只是“用户 -> 角色/权限/数据范围”的
     * 快照，把每请求的数据库往返降到 TTL 内 0 次。
     */
    class principal_cache
    {
    public:
        explicit principal_cache(std::chrono::seconds ttl)
            : ttl_{ttl}
        {
        }

        [[nodiscard]] auto find(std::int64_t user_id) const
            -> std::shared_ptr<const authenticated_context>
        {
            if (ttl_.count() <= 0)
                return nullptr;
            const auto found = entries_.find(user_id);
            if (!found || (*found)->expires_at <= std::chrono::steady_clock::now())
                return nullptr;
            return (*found)->context;
        }

        void store(std::int64_t user_id,
            std::shared_ptr<const authenticated_context> context)
        {
            if (ttl_.count() <= 0)
                return;
            const auto now = std::chrono::steady_clock::now();
            entries_.insert_or_assign(user_id,
                std::make_shared<const entry>(entry{
                    .context = std::move(context), .expires_at = now + ttl_}));
            sweep(now);
        }

        void erase(std::int64_t user_id)
        {
            (void)entries_.erase(user_id);
        }

    private:
        struct entry
        {
            std::shared_ptr<const authenticated_context> context;
            std::chrono::steady_clock::time_point expires_at;
        };

        /// @brief 每个 TTL 周期最多清理一次过期项，保证缓存规模只跟活跃用户数相关。
        void sweep(std::chrono::steady_clock::time_point now)
        {
            const auto now_ticks = now.time_since_epoch().count();
            auto previous = last_sweep_.load(std::memory_order_acquire);
            const auto period = std::chrono::duration_cast<std::chrono::steady_clock::duration>(ttl_)
                                    .count();
            if (now_ticks - previous < period ||
                !last_sweep_.compare_exchange_strong(previous, now_ticks,
                    std::memory_order_acq_rel, std::memory_order_acquire))
                return;
            (void)entries_.erase_if(
                [now](const std::int64_t&, const std::shared_ptr<const entry>& item)
                { return item->expires_at <= now; });
        }

        std::chrono::seconds ttl_;
        cnetmod::concurrent_containers::striped_hash_map<std::int64_t,
            std::shared_ptr<const entry>>
            entries_;
        std::atomic<std::chrono::steady_clock::rep> last_sweep_{};
    };

    struct security_state
    {
        explicit security_state(jwt_security_options configured)
            : options{std::move(configured)},
              principals{options.principal_cache_ttl}
        {
        }

        jwt_security_options options;
        principal_cache principals;
    };

    /// @brief 取授权快照：先查缓存，未命中再经 loader 读库并回填。
    [[nodiscard]] auto principal_for(security_state& state, std::int64_t user_id)
        -> cnetmod::task<std::expected<std::shared_ptr<const authenticated_context>, error_code>>
    {
        if (auto cached = state.principals.find(user_id))
            co_return cached;
        auto loaded = co_await state.options.load_principal(user_id);
        if (!loaded)
            co_return std::unexpected{loaded.error()};
        auto shared = std::make_shared<const authenticated_context>(std::move(*loaded));
        state.principals.store(user_id, shared);
        co_return shared;
    }

    /**
     * @brief 校验令牌并把身份绑定到请求作用域。
     *
     * 每请求一次 Redis 往返（会话读取 + 续期）；授权快照命中缓存时不访问数据库。
     */
    [[nodiscard]] auto authenticate_request(security_state& state,
        cnetmod::http::request_context& request, std::string_view token)
        -> cnetmod::task<std::expected<void, cnetmod::jwt_auth_failure>>
    {
        const auto& settings = *state.options.settings;
        auto& executor = *state.options.executor;
        auto claims = co_await cnetmod::security::verify_jwt(executor.cpu_pool(),
            executor.event_loop(), token, settings.secret);
        if (!claims || claims->issuer != settings.issuer)
            co_return std::unexpected(reject(request, error_code::auth_token_invalid));
        if (cnetmod::security::is_jwt_expired(*claims))
            co_return std::unexpected(reject(request, error_code::auth_token_expired));

        const auto id = parse_user_id(claims->subject);
        const auto session = claims->custom.find("sid");
        if (!id || session == claims->custom.end() ||
            !well_formed_session_id(session->second))
            co_return std::unexpected(reject(request, error_code::auth_token_invalid));

        auto& sessions = *state.options.sessions;
        auto record = co_await sessions.touch(session->second);
        if (!record)
            co_return std::unexpected(reject(request, record.error()));
        if (!*record || (*record)->user_id != *id || (*record)->username.empty())
            co_return std::unexpected(reject(request, error_code::auth_token_invalid));

        auto principal = co_await principal_for(state, *id);
        if (!principal)
        {
            // 主体不再有效（删除 / 停用）：撤销会话，后续请求不必再查库。
            if (principal.error() != error_code::upstream_unavailable)
                (void)co_await sessions.revoke(session->second);
            co_return std::unexpected(reject(request, principal.error()));
        }
        if ((*record)->username != (*principal)->user.username ||
            (*record)->department_id != (*principal)->user.department_id)
        {
            // 会话签发后身份已变化（改名 / 调部门）：旧会话作废，要求重新登录。
            state.principals.erase(*id);
            (void)co_await sessions.revoke(session->second);
            co_return std::unexpected(reject(request, error_code::auth_token_invalid));
        }

        request.scope().bind(std::make_shared<current_user>((*principal)->user));
        request.scope().bind(std::make_shared<jwt_session_id>(
            jwt_session_id{.value = session->second}));
        request.scope().bind(std::make_shared<cnetmod::orm::data_permission_scope>(
            (*principal)->data_permission));
        co_return {};
    }

} // namespace

auto jwt_session_key(std::string_view session_id) -> std::string
{
    return std::string{"cnetnexus:jwt:session:{sessions}:"} +
        std::string{session_id};
}

jwt_session_store::jwt_session_store(
    std::shared_ptr<cnetmod::redis::redis_template> redis,
    std::chrono::seconds idle_timeout)
    : redis_{std::move(redis)},
      idle_timeout_{idle_timeout}
{
    if (!redis_)
        throw std::invalid_argument{"jwt session store requires a redis template"};
    if (idle_timeout_.count() <= 0)
        throw std::invalid_argument{"jwt session idle timeout must be positive"};
}

auto jwt_session_store::open(const jwt_session_record& record)
    -> cnetmod::task<std::expected<std::string, error_code>>
{
    auto encoded = cnetmod::json::write(record);
    if (!encoded)
        co_return std::unexpected{error_code::system_internal_error};
    auto id = cnetmod::security::generate_secure_token(session_id_bytes);
    if (auto stored = co_await redis_->set(jwt_session_key(id), *encoded, idle_timeout_);
        !stored)
    {
        logger::warn{"jwt session store unavailable: {}", stored.error().message()};
        co_return std::unexpected{error_code::upstream_unavailable};
    }
    co_return id;
}

auto jwt_session_store::touch(std::string_view session_id)
    -> cnetmod::task<std::expected<std::optional<jwt_session_record>, error_code>>
{
    auto value = co_await redis_->getex(
        jwt_session_key(session_id), idle_timeout_);
    if (!value)
    {
        logger::warn{"jwt session store unavailable: {}",
            value.error().message()};
        co_return std::unexpected{error_code::upstream_unavailable};
    }
    if (!*value)
        co_return std::optional<jwt_session_record>{};
    auto record = cnetmod::json::parse<jwt_session_record>(**value);
    if (!record)
        co_return std::optional<jwt_session_record>{};
    co_return std::optional<jwt_session_record>{std::move(*record)};
}

auto jwt_session_store::rotate(std::string_view session_id,
    const jwt_session_record& record)
    -> cnetmod::task<std::expected<std::string, error_code>>
{
    auto encoded = cnetmod::json::write(record);
    if (!encoded)
        co_return std::unexpected{error_code::system_internal_error};

    auto replacement = cnetmod::security::generate_secure_token(
        session_id_bytes);
    const std::vector<std::string> keys{
        jwt_session_key(session_id), jwt_session_key(replacement)};
    const std::vector<std::string> arguments{
        std::move(*encoded), std::to_string(idle_timeout_.count())};
    auto rotated = co_await redis_->eval_integer(
        rotate_session_script, keys, arguments);
    if (!rotated)
    {
        logger::warn{"jwt session rotation unavailable: {}",
            rotated.error().message()};
        co_return std::unexpected{error_code::upstream_unavailable};
    }
    if (*rotated == 0)
        co_return std::unexpected{error_code::auth_token_invalid};
    co_return replacement;
}

auto jwt_session_store::revoke(std::string_view session_id)
    -> cnetmod::task<std::expected<void, error_code>>
{
    if (auto removed = co_await redis_->del(jwt_session_key(session_id)); !removed)
    {
        logger::warn{"jwt session store unavailable: {}", removed.error().message()};
        co_return std::unexpected{error_code::upstream_unavailable};
    }
    co_return std::expected<void, error_code>{};
}

auto jwt_security(jwt_security_options options) -> cnetmod::http::middleware_fn
{
    if (options.settings == nullptr || options.executor == nullptr ||
        !options.sessions || !options.load_principal)
        throw std::invalid_argument(
            "jwt security requires settings, executor, session store and principal loader");
    auto state = std::make_shared<security_state>(std::move(options));
    auto authenticate = [state](cnetmod::http::request_context& request,
                            std::string_view token)
        -> cnetmod::task<std::expected<void, cnetmod::jwt_auth_failure>>
    {
        return authenticate_request(*state, request, token);
    };

    auto on_failure = [](cnetmod::http::request_context& request,
                          const cnetmod::jwt_auth_failure& failure)
    {
        const auto* precise = request.scope().find<authentication_failure_code>();
        cnetnexus::framework::web::respond_error(request,
            precise != nullptr ? precise->code : code_for(failure.reason));
    };

    // Route policy comes from endpoint metadata; unmatched paths reach the
    // router and answer 404 or 405.
    return cnetmod::jwt_auth({
        .authenticate_async = std::move(authenticate),
        .on_failure = std::move(on_failure),
    });
}

} // namespace cnetnexus::security
