#include "suppress.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::string_view> split(std::string_view text) {
    std::vector<std::string_view> lines;
    while (!text.empty()) {
        auto const end = text.find('\n');
        lines.push_back(text.substr(0, end));
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
    }
    return lines;
}

Diagnostic finding(std::string rule, std::size_t line, std::string file = "models/a.yang") {
    return {std::move(file), std::move(rule), "текст", line, 15};
}

} // namespace

/**
 * Метка в комментарии на строке pattern: все правила или только перечисленные.
 */
TEST(Suppress, MarkerOnSameLine) {
    auto const text =
        "leaf a { type string {\n"
        "  pattern '^x$'; // yang-pattern-lint-disable anchors, mismatch\n"
        "  pattern '^y$'; /* yang-pattern-lint-disable */\n"
        "} }\n";
    auto const lines = split(text);
    Suppressions const s;
    Pattern const first{"^x$", "'^x$'", 2, 11, 2};
    EXPECT_TRUE(is_suppressed(s, finding("anchors", 2), first, lines));
    EXPECT_TRUE(is_suppressed(s, finding("mismatch", 2), first, lines));
    EXPECT_FALSE(is_suppressed(s, finding("posix", 2), first, lines)) << "posix не перечислен в метке";
    Pattern const second{"^y$", "'^y$'", 3, 11, 3};
    EXPECT_TRUE(is_suppressed(s, finding("posix", 3), second, lines)) << "метка без правил отключает все";
}

/**
 * Метка на отдельной строке над pattern действует, хвостовая метка предыдущего оператора нет.
 */
TEST(Suppress, MarkerAbove) {
    auto const text =
        "  // yang-pattern-lint-disable mismatch\n"
        "  pattern '^x$';\n"
        "  pattern 'a'; // yang-pattern-lint-disable\n"
        "  pattern '^y$';\n";
    auto const lines = split(text);
    Suppressions const s;
    EXPECT_TRUE(is_suppressed(s, finding("mismatch", 2), {"^x$", "'^x$'", 2, 11, 2}, lines));
    EXPECT_FALSE(is_suppressed(s, finding("mismatch", 4), {"^y$", "'^y$'", 4, 11, 4}, lines))
        << "метка в конце строки 3 относится к своему pattern, не к следующему";
}

/**
 * Метка внутри строки, а не в комментарии, не действует.
 */
TEST(Suppress, MarkerOutsideComment) {
    auto const lines = split("  pattern 'yang-pattern-lint-disable';\n");
    EXPECT_FALSE(is_suppressed({}, finding("anchors", 1), {"x", "'x'", 1, 11, 1}, lines));
}

/**
 * Ключ --disable и файл исключений с glob по пути и его хвостам.
 */
TEST(Suppress, DisabledAndIgnoreFile) {
    Suppressions s;
    s.disabled = split_rules("escape,anchors");
    s.entries = parse_ignore_file(
        "# комментарий\n"
        "\n"
        "vendor/*.yang\n"
        "a.yang mismatch,posix\r\n");
    ASSERT_EQ(s.entries.size(), 2U) << "комментарий и пустая строка пропускаются";
    EXPECT_EQ(s.entries[1].glob, "a.yang");
    EXPECT_THAT(s.entries[1].rules, testing::ElementsAre("mismatch", "posix")) << "запятая и \\r в конце";

    std::vector<std::string_view> const none;
    Pattern const p{"x", "'x'", 1, 1, 1};
    EXPECT_TRUE(is_suppressed(s, finding("anchors", 1), p, none)) << "--disable";
    EXPECT_TRUE(is_suppressed(s, finding("mismatch", 1), p, none)) << "a.yang совпадает с хвостом models/a.yang";
    EXPECT_FALSE(is_suppressed(s, finding("xsd", 1), p, none)) << "xsd для a.yang не подавлен";
    EXPECT_TRUE(is_suppressed(s, finding("xsd", 1, "src/vendor/b.yang"), p, none)) << "glob без правил: все правила";
    EXPECT_FALSE(is_suppressed(s, finding("xsd", 1, "models/b.yang"), p, none));
}
