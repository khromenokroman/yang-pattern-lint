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
