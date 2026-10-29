#include "yang.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::vector<std::string> values(std::string_view text) {
    std::vector<std::string> out;
    for (auto const &p : extract_patterns(text)) {
        out.push_back(p.value);
    }
    return out;
}

} // namespace

/**
 * Одинарные кавычки: содержимое берётся как есть, обратный слеш не обрабатывается.
 */
TEST(Yang, SingleQuotedAsIs) {
    auto const text = R"(leaf name { type string { pattern '[a-z\-]+\\d'; } })";
    EXPECT_THAT(values(text), testing::ElementsAre(R"([a-z\-]+\\d)")) << "текст: " << text;
}

/**
 * Двойные кавычки: \\ и \" раскрываются, неизвестный escape остаётся как есть (YANG 1.0).
 */
TEST(Yang, DoubleQuotedEscapes) {
    auto const text = R"(pattern "a\\b\"c\.d";)";
    EXPECT_THAT(values(text), testing::ElementsAre(R"(a\b"c\.d)")) << "текст: " << text;
}

/**
 * Склейка через '+' на нескольких строках, позиция у первой части.
 */
TEST(Yang, Concatenation) {
    auto const text =
        "container c {\n"
        "  leaf v {\n"
        "    type string {\n"
        "      pattern '[0-9]+'\n"
        "            + \"(:[0-9]+)?\";\n"
        "    }\n"
        "  }\n"
        "}\n";
    auto const patterns = extract_patterns(text);
    ASSERT_EQ(patterns.size(), 1U) << "текст:\n" << text;
    EXPECT_EQ(patterns[0].value, "[0-9]+(:[0-9]+)?");
    EXPECT_EQ(patterns[0].line, 4U) << "строка первой части";
    EXPECT_EQ(patterns[0].column, 15U) << "столбец первой кавычки";
    EXPECT_EQ(patterns[0].literal, "'[0-9]+'\n            + \"(:[0-9]+)?\"") << "literal от первой до последней кавычки";
}

/**
 * Склейка без пробелов вокруг '+', так вставляют апостроф в строку в одинарных кавычках.
 * Слово без кавычек, начинающееся с '+', остаётся словом.
 */
TEST(Yang, ConcatenationWithoutSpaces) {
    auto const text = R"(pattern '[a-z'+"'"+'0-9]+';
pattern +5;)";
    EXPECT_THAT(values(text), testing::ElementsAre("[a-z'0-9]+", "+5")) << "текст:\n" << text;
}

/**
 * Аргумент на следующей строке после pattern, без кавычек тоже допустим.
 */
TEST(Yang, ArgumentOnNextLineAndUnquoted) {
    auto const text = "pattern\n    '[a-f]+';\npattern abc;\n";
    EXPECT_THAT(values(text), testing::ElementsAre("[a-f]+", "abc")) << "текст:\n" << text;
}

/**
 * Слово pattern в строках, комментариях и в роли аргумента не является оператором.
 */
TEST(Yang, PatternWordNotStatement) {
    auto const text =
        "// pattern 'x';\n"
        "/* pattern 'y'; */\n"
        "description \"use pattern 'z' here\";\n"
        "typedef pattern { type string; }\n"
        "leaf l { type string { pattern 'ok'; } }\n";
    EXPECT_THAT(values(text), testing::ElementsAre("ok")) << "текст:\n" << text;
}

/**
 * Многострочная двойная строка: отступ до столбца кавычки снимается, хвостовые пробелы убираются.
 */
TEST(Yang, DoubleQuotedMultilineIndent) {
    auto const text =
        "  pattern \"ab  \n"
        "            cd\";\n";
    EXPECT_THAT(values(text), testing::ElementsAre("ab\n cd")) << "кавычка в столбце 11: снимаются 11 пробелов из 12, хвост 'ab  ' обрезается";
}

/**
 * Комментарий сразу после слова без пробела не входит в слово.
 */
TEST(Yang, CommentAfterWord) {
    auto const text = "pattern abc// комментарий\n;";
    EXPECT_THAT(values(text), testing::ElementsAre("abc")) << "текст: " << text;
}

/**
 * Ошибки разбора с позицией.
 */
TEST(Yang, ParseErrors) {
    struct Case {
        std::string_view text;
        std::string_view reason;
        std::size_t line;
    };
    std::array<Case, 4> const cases{{
        {"leaf a {\n pattern '[a-z;\n}", "незакрытая строка", 2},
        {"/* x\n pattern 'a';", "незакрытый комментарий", 1},
        {"leaf a { pattern ; }", "нет аргумента", 1},
        {"pattern 'a' +\n ;", "после '+'", 1},
    }};
    for (auto const &c : cases) {
        SCOPED_TRACE(std::string("текст: ") + std::string(c.text));
        try {
            (void)extract_patterns(c.text);
            ADD_FAILURE() << "ожидалась ошибка: " << c.reason;
        } catch (ParseError const &e) {
            EXPECT_THAT(e.what(), testing::HasSubstr(std::string(c.reason)));
            EXPECT_EQ(e.line(), c.line) << "строка ошибки";
        }
    }
}

/**
 * Обход каталога: только *.yang, рекурсивно, без повторов, по порядку.
 */
TEST(Yang, CollectFiles) {
    auto const root = std::filesystem::temp_directory_path() / "ypl_collect_test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "sub");
    for (auto const *name : {"b.yang", "a.yang", "notes.txt", "sub/c.yang"}) {
        std::ofstream{root / name} << "module m {}";
    }
    std::array<std::filesystem::path, 2> const paths{root, root / "a.yang"};
    auto const files = collect_yang_files(paths);
    EXPECT_THAT(files, testing::ElementsAre(root / "a.yang", root / "b.yang", root / "sub/c.yang"))
        << "каталог и файл из него же должны дать один a.yang";

    std::array<std::filesystem::path, 1> const missing{root / "nope"};
    EXPECT_THROW((void)collect_yang_files(missing), std::filesystem::filesystem_error) << "несуществующий путь";
    std::filesystem::remove_all(root);
}
