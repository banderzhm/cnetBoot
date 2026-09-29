/**
 * @brief Audit contract shared by every module that records write operations.
 *
 * Audit rows are append-only. Passwords, tokens and full sensitive bodies must
 * never reach an audit detail string.
 */
export module cnetboot.framework.audit;

import std;
import cnetmod.coro.task;
import cnetboot.framework.error;

export namespace cnetboot::framework {

/** @brief Audit action classification; every write path records one row. */
inline constexpr std::string_view audit_action_login{"login"};
inline constexpr std::string_view audit_action_logout{"logout"};
inline constexpr std::string_view audit_action_create{"create"};
inline constexpr std::string_view audit_action_update{"update"};
inline constexpr std::string_view audit_action_delete{"delete"};
inline constexpr std::string_view audit_action_password{"password"};

/** @brief Operator and request provenance for one audited operation.
 *
 *  The HTTP boundary assembles it from the request context; services consume it
 *  and never read HTTP objects themselves.
 */
struct audit_actor
{
    std::uint64_t user_id{}; ///< operator id; 0 for anonymous operations
    std::string username;    ///< operator login name
    std::string ip;          ///< source address
    std::string user_agent;  ///< source user agent
    std::string request_id;  ///< X-Request-Id
    std::string method;      ///< HTTP method
    std::string path;        ///< request path
};

/**
 * @brief Records append-only audit rows.
 *
 * Modules depend on this contract rather than a concrete storage choice, so the
 * composition root decides whether audit rows reach MySQL or memory.
 */
class audit_log_service
{
public:
    audit_log_service() = default;
    audit_log_service(const audit_log_service&) = delete;
    audit_log_service(audit_log_service&&) = delete;
    audit_log_service& operator=(const audit_log_service&) = delete;
    audit_log_service& operator=(audit_log_service&&) = delete;
    virtual ~audit_log_service() = default;

    /** @brief Appends one audit row.
     *  @param actor operator and request provenance
     *  @param action login / logout / create / update / delete / password
     *  @param resource audited resource name, e.g. sys_user / chat_session
     *  @param status_code business result status code
     *  @param detail already-redacted change summary; must not carry secrets
     */
    [[nodiscard]] virtual auto record(const audit_actor& actor,
        std::string_view action, std::string_view resource, int status_code,
        std::string detail = {})
        -> cnetmod::task<std::expected<void, error::error_code>> = 0;
};

} // namespace cnetboot::framework
