#include "checks.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

/**
 * Корректный паттерн: находок нет.
 */
TEST(Checks, ValidPatternHasNoFindings) {
    Pattern const p{"[a-z][a-z0-9_-]{0,62}", "'[a-z][a-z0-9_-]{0,62}'", 3, 15};
    EXPECT_THAT(check_pattern("m.yang", p), testing::IsEmpty()) << "паттерн " << p.value;
}

/**
 * Паттерн не компилируется libxml2: ошибка xsd с позицией аргумента и текстом libxml2.
 */
TEST(Checks, XsdCompileError) {
    Pattern const p{R"(([a-z]|\)+)", R"('([a-z]|\)+')", 7, 21};
    auto const findings = check_pattern("m.yang", p);
    ASSERT_EQ(findings.size(), 1U) << "паттерн " << p.value;
    auto const &d = findings.front();
    EXPECT_EQ(d.rule, "xsd");
    EXPECT_EQ(d.severity, Diagnostic::Severity::ERROR);
    EXPECT_EQ(d.line, 7U);
    EXPECT_EQ(d.column, 21U);
    EXPECT_THAT(d.message, testing::HasSubstr("не компилируется по XSD"));
    EXPECT_THAT(format_diagnostic(d), testing::StartsWith("m.yang:7:21: ошибка [xsd]: ")) << "формат строки";
}

namespace {

std::vector<std::string> rules_of(Pattern const &p) {
    std::vector<std::string> rules;
    for (auto const &d : check_pattern("m.yang", p)) {
        rules.push_back(d.rule + (d.severity == Diagnostic::Severity::WARNING ? ":w" : ":e"));
    }
    return rules;
}

} // namespace

/**
 * Предупреждения по записи: \\ в значении, ^ и $ по краям, неизвестный escape в двойных кавычках.
 */
TEST(Checks, StyleWarnings) {
    EXPECT_THAT(rules_of({R"([a-z\\]+)", R"('[a-z\\]+')", 1, 1}), testing::Contains("escaped-backslash:w")) << "буквальный обратный слеш";
    EXPECT_THAT(rules_of({"^[a-z]+", "'^[a-z]+'", 1, 1}), testing::Contains("anchors:w")) << "^ в начале";
    EXPECT_THAT(rules_of({"[a-z]+$", "'[a-z]+$'", 1, 1}), testing::Contains("anchors:w")) << "$ в конце";
    EXPECT_THAT(rules_of({R"([a-z]+\$)", R"('[a-z]+\$')", 1, 1}), testing::Not(testing::Contains("anchors:w"))) << "экранированный $ не якорь";
    EXPECT_THAT(rules_of({R"([0-9]+\.[0-9]+)", R"("[0-9]+\.[0-9]+")", 1, 1}), testing::Contains("escape:w")) << "\\. в двойных кавычках";
    EXPECT_THAT(rules_of({R"([0-9]+\.[0-9]+)", R"('[0-9]+\.[0-9]+')", 1, 1}), testing::IsEmpty())
        << "\\. в одинарных кавычках допустим, обычный \\d и \\. не предупреждаются";
    EXPECT_THAT(rules_of({R"(a\d)", R"("a\\d")", 1, 1}), testing::IsEmpty()) << "\\\\ в двойных кавычках допустим";
}
