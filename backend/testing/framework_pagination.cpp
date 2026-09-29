import cnetboot.framework.pagination;

auto main() -> int
{
    namespace pagination = cnetboot::framework::pagination;

    const auto defaults = pagination::normalize({.page = 0, .page_size = 0});
    if (defaults.page != 1 ||
        defaults.page_size != pagination::default_page_size)
        return 1;

    const auto maximum = pagination::normalize(
        {.page = 7, .page_size = pagination::max_page_size + 1});
    if (maximum.page != 7 || maximum.page_size != pagination::max_page_size)
        return 2;

    const auto unchanged = pagination::normalize({.page = 3, .page_size = 25});
    return unchanged.page == 3 && unchanged.page_size == 25 ? 0 : 3;
}
