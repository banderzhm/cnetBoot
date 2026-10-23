import std;
import cnetboot.framework.web.query;

namespace query = cnetboot::framework::web::query;

// Query parsing contract: percent decoding, '+', integers, and integer lists.
auto main() -> int
{
    constexpr std::string_view raw = "a=1&b=hello%20world&c=payment&d=x+y&e=&n=42&bad=4x&l=1,3";

    if (query::value(raw, "a").value_or("") != "1")
        return 1;
    if (query::value(raw, "b").value_or("") != "hello world")
        return 2;
    if (query::value(raw, "c").value_or("") != "payment")
        return 3;
    if (query::value(raw, "d").value_or("") != "x y")
        return 4;
    if (!query::value(raw, "e") || !query::value(raw, "e")->empty())
        return 5;
    if (query::value(raw, "missing").has_value())
        return 6;

    if (query::integer(raw, "n").value_or(0) != 42)
        return 7;
    if (query::integer(raw, "bad").has_value() || query::integer(raw, "missing").has_value())
        return 8;

    if (query::integer_list(raw, "l").value_or(std::vector<int>{}) != std::vector<int>{1, 3})
        return 9;
    if (!query::integer_list(raw, "missing") || !query::integer_list(raw, "missing")->empty())
        return 10;
    if (query::integer_list("l=1,x", "l").has_value())
        return 11;
    return 0;
}
