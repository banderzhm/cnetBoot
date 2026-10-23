
export module cnetboot.system.service.login_guard;

import std;
import cnetmod.coro.task;
import cnetmod.protocol.redis;
import cnetboot.framework.error;

export namespace cnetboot::system {


struct login_protection_options
{
    std::uint32_t max_account_failures{5};
    std::uint32_t max_client_failures{20};
    std::int64_t window_seconds{900};
    std::int64_t lock_seconds{900};
};


[[nodiscard]] auto validate_login_protection(const login_protection_options& options)
    -> std::expected<void, std::string>;

class login_guard
{
public:
    login_guard(std::shared_ptr<cnetmod::redis::redis_template> store,
        login_protection_options options);


    [[nodiscard]] auto admit(std::string_view username, std::string_view client)
        -> cnetmod::task<std::expected<void, cnetboot::framework::error::error_code>>;


    [[nodiscard]] auto record_failure(std::string_view username, std::string_view client)
        -> cnetmod::task<void>;


    [[nodiscard]] auto record_success(std::string_view username) -> cnetmod::task<void>;

private:
    [[nodiscard]] auto count(std::string key, std::uint32_t limit, std::string lock)
        -> cnetmod::task<void>;

    std::shared_ptr<cnetmod::redis::redis_template> store_;
    login_protection_options options_;
};

} // namespace cnetboot::system
