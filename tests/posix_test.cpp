#include "posix.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>

#include "checks.hpp"
#include "xsd.hpp"

/**
 * Перевод clixon: \d раскрывается в класс, $ не в конце экранируется.
 */
TEST(Posix, Translation) {
    struct Case {
        char const *xsd;
        char const *posix;
    };
    std::array<Case, 3> const cases{{
        {"[a-z]+", "[a-z]+"},
        {R"(\d{1,3})", "[0-9]{1,3}"},
        {"a$b", R"(a\$b)"},
    }};
    for (auto const &c : cases) {
        EXPECT_EQ(xsd_to_posix(c.xsd), c.posix) << "XSD: " << c.xsd;
    }
}

/**
 * Обычный паттерн: CLI принимает и отклоняет те же строки, что и XSD.
 */
TEST(Posix, MatchesLikeXsd) {
    PosixRegex const regex{"[a-z]+(-[0-9]+)?"};
    EXPECT_TRUE(regex.matches("abc"));
    EXPECT_TRUE(regex.matches("abc-12"));
    EXPECT_FALSE(regex.matches("abc-")) << "якоря ^(...)$ добавлены cligen";
    EXPECT_FALSE(regex.matches("x abc")) << "якорь в начале";
}

/**
 * \] внутри класса: XSD понимает экранирование, а в POSIX класс закрывается на первой ']'.
 * Паттерн компилируется в обоих, но CLI отклоняет обычные значения.
 */
TEST(Posix, EscapedBracketInClassDiverges) {
    auto const *xsd = R"([a-z\[\]]+)";
    XsdRegex const x{xsd};
    PosixRegex const p{xsd};
    EXPECT_TRUE(x.matches("abc")) << "XSD принимает abc";
    EXPECT_FALSE(p.matches("abc")) << "CLI отклоняет abc, POSIX: " << p.posix();
}

/**
 * Корректные по XSD паттерны, которые CLI не может скомпилировать: вычитание классов XSD
 * и \P{...} clixon не переводит, а в POSIX ERE такого синтаксиса нет.
 */
TEST(Posix, CompileErrors) {
    std::array<char const *, 2> const cases{R"([a-z-[aeiou]]+)", R"(\P{L})"};
    for (auto const *xsd : cases) {
        EXPECT_NO_THROW(XsdRegex{xsd}) << "по XSD паттерн корректен: " << xsd;
        try {
            PosixRegex const regex{xsd};
            ADD_FAILURE() << "ожидалась ошибка компиляции в CLI для " << xsd;
        } catch (PosixError const &e) {
            EXPECT_THAT(e.what(), testing::HasSubstr("не компилируется")) << "текст: " << e.what();
        }
    }
}

/**
 * Правило posix в проверках: ошибка с позицией и переведённой строкой, xsd при этом нет.
 */
TEST(Posix, CheckRule) {
    Pattern const p{"[a-z-[aeiou]]+", "'[a-z-[aeiou]]+'", 4, 17};
    auto const findings = check_pattern("m.yang", p);
    ASSERT_EQ(findings.size(), 1U) << "паттерн " << p.value;
    EXPECT_EQ(findings[0].rule, "posix");
    EXPECT_EQ(findings[0].line, 4U);
    EXPECT_THAT(findings[0].message, testing::HasSubstr("CLI clixon"));
    EXPECT_THAT(findings[0].message, testing::HasSubstr("'[a-z-[aeiou]]+'")) << "переведённая строка в тексте";
}
