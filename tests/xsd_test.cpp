#include "xsd.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <string>

/**
 * Корректные паттерны компилируются, сопоставление идёт по всей строке.
 */
TEST(Xsd, MatchesWholeString) {
    XsdRegex const regex{"[a-z]+[0-9]?"};
    struct Case {
        char const *value;
        bool expected;
    };
    std::array<Case, 5> const cases{{
        {"abc", true},
        {"abc7", true},
        {"abc77", false},
        {"x abc", false},
        {"", false},
    }};
    for (auto const &c : cases) {
        EXPECT_EQ(regex.matches(c.value), c.expected) << "паттерн [a-z]+[0-9]? на '" << c.value << "'";
    }
}

/**
 * В XSD ^ и $ обычные символы, а не якоря.
 */
TEST(Xsd, CaretAndDollarAreLiterals) {
    XsdRegex const regex{"^a$"};
    EXPECT_TRUE(regex.matches("^a$")) << "строка с символами ^ и $";
    EXPECT_FALSE(regex.matches("a")) << "без ^ и $ не подходит";
}

/**
 * Экранирование внутри класса: \[ \] \\ и - в конце.
 */
TEST(Xsd, EscapesInClass) {
    XsdRegex const regex{R"([a-z\[\]\\-]+)"};
    EXPECT_TRUE(regex.matches(R"(a[b]c\-)")) << "все символы класса";
    EXPECT_FALSE(regex.matches("a b")) << "пробела в классе нет";
}

/**
 * Некомпилируемые паттерны: исключение с сообщением libxml2.
 */
TEST(Xsd, CompileErrors) {
    // a{2,1} (min больше max) libxml2 2.9 принимает, поэтому здесь его нет
    std::array<char const *, 3> const bad{
        R"(([a-z]|\)+)", // экранированная скобка, группа не закрыта
        "[a-z",          // класс не закрыт
        "(abc",          // группа не закрыта
    };
    for (auto const *pattern : bad) {
        try {
            XsdRegex const regex{pattern};
            ADD_FAILURE() << "ожидалась ошибка компиляции: " << pattern;
        } catch (XsdError const &e) {
            EXPECT_THAT(std::string(e.what()), testing::Not(testing::IsEmpty())) << "пустое сообщение для " << pattern;
        }
    }
}
