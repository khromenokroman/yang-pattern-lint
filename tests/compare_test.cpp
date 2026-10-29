#include "compare.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <string>

#include "checks.hpp"

namespace {

Mismatches compare(char const *xsd) { return find_mismatches(xsd, YangRegex{xsd}, PosixRegex{xsd}); }

} // namespace

/**
 * Обычные паттерны: XSD (libyang) и CLI согласны.
 */
TEST(Compare, NoMismatchForPlainPatterns) {
    std::array<char const *, 5> const patterns{
        "[a-z][a-z0-9_-]{0,15}", R"(\d{2,4}(:\d+)?)", "(red|green|blue)+", "[A-Z]{2}[0-9]{4}", R"(([a-z]|\[|\])+)",
    };
    for (auto const *pattern : patterns) {
        auto const m = compare(pattern);
        EXPECT_FALSE(m.rejected_by_cli) << "паттерн " << pattern << ", CLI отклоняет '" << m.rejected_by_cli->value << "'";
        EXPECT_FALSE(m.accepted_by_cli) << "паттерн " << pattern << ", CLI принимает '" << m.accepted_by_cli->value << "'";
    }
}

/**
 * Известные расхождения перевода clixon: находится строка, которую XSD принимает, а CLI нет.
 */
TEST(Compare, RejectedByCli) {
    struct Case {
        char const *xsd;
        char const *why;
    };
    std::array<Case, 3> const cases{{
        {R"([a-z\[\]]+)", "\\] внутри класса в POSIX закрывает класс"},
        {R"(\p{Lu}+)", "\\p{Lu} переводится без скобок класса"},
        {R"([^\d]+)", "вложенный класс [^[0-9]] ломает отрицание"},
    }};
    for (auto const &c : cases) {
        auto const m = compare(c.xsd);
        EXPECT_TRUE(m.rejected_by_cli.has_value()) << "паттерн " << c.xsd << ": " << c.why;
    }
}

/**
 * ^ и $ в XSD обычные символы, а cligen считает их якорями: CLI принимает строку без них.
 */
TEST(Compare, CaretDollarAreAnchorsInCli) {
    auto const m = compare("^a$");
    ASSERT_TRUE(m.accepted_by_cli.has_value()) << "ожидалась строка, которую CLI принимает, а XSD нет";
    EXPECT_EQ(m.accepted_by_cli->value, "a") << "самый короткий пример";
    ASSERT_TRUE(m.rejected_by_cli.has_value());
    EXPECT_EQ(m.rejected_by_cli->value, "^a$") << "по XSD подходит только строка с символами ^ и $";
}

/**
 * Правило mismatch: одна находка с обоими примерами и переведённой строкой.
 */
TEST(Compare, CheckRule) {
    Pattern const p{"^a$", "'^a$'", 2, 9};
    auto const findings = check_pattern("m.yang", p);
    auto const it = std::ranges::find(findings, std::string("mismatch"), &Diagnostic::rule);
    ASSERT_NE(it, findings.end()) << "ожидалась находка mismatch";
    EXPECT_THAT(it->message, testing::HasSubstr("'^a$' по XSD подходит, CLI отклоняет"));
    EXPECT_THAT(it->message, testing::HasSubstr("'a' по XSD не подходит, CLI принимает"));
}

/**
 * Результат детерминирован: два прогона дают одинаковые примеры.
 */
TEST(Compare, Deterministic) {
    auto const first = compare(R"([a-z\[\]]+)");
    auto const second = compare(R"([a-z\[\]]+)");
    ASSERT_TRUE(first.rejected_by_cli && second.rejected_by_cli);
    EXPECT_EQ(first.rejected_by_cli->value, second.rejected_by_cli->value);
}
