export module cnetboot.framework.pagination;

import std;

export namespace cnetboot::framework::pagination {

inline constexpr int default_page_size = 20;
inline constexpr int max_page_size = 100;

struct page_request
{
    int page{1};
    int page_size{default_page_size};
};

template <typename T> struct page_result
{
    std::vector<T> items;
    std::int64_t total{};
    int page{1};
    int page_size{default_page_size};
};

[[nodiscard]] inline auto normalize(const page_request& request) noexcept
    -> page_request
{
    page_request normalized = request;
    if (normalized.page < 1)
        normalized.page = 1;
    if (normalized.page_size < 1)
        normalized.page_size = default_page_size;
    if (normalized.page_size > max_page_size)
        normalized.page_size = max_page_size;
    return normalized;
}

} // namespace cnetboot::framework::pagination
