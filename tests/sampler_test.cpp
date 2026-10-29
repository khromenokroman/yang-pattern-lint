#include "sampler.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <random>

#include "xsd.hpp"

/**
 * Строки генератора подходят под паттерн по libxml2 для типичных конструкций.
 */
TEST(Sampler, SamplesMatchXsd) {
    std::array<char const *, 10> const patterns{
        "[a-z][a-z0-9_-]{0,15}",
        R"(((25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|1[0-9]{2}|[1-9]?[0-9]))",
        R"(\d{2,4}(:\d+)?)",
        "(red|green|blue)+",
        "[^ ]+",
        "[a-z-[aeiou]]+",
        R"(\i\c*)",
        R"(\p{Lu}\p{Ll}*)",
        R"([\-+]?[0-9]+)",
        "x{3}",
    };
    std::mt19937 rng{42};
    for (auto const *pattern : patterns) {
        Sampler const sampler{pattern};
        XsdRegex const regex{pattern};
        for (int i = 0; i < 50; ++i) {
            auto const value = sampler.sample(rng);
            EXPECT_TRUE(regex.matches(value)) << "паттерн " << pattern << ", строка генератора '" << value << "'";
        }
    }
}

/**
 * Пустой набор символов (только управляющие) не даёт примера.
 */
TEST(Sampler, EmptySetGivesNoSample) {
    Sampler const sampler{R"(a\nb)"};
    std::mt19937 rng{1};
    EXPECT_EQ(sampler.sample(rng), "") << "\\n не генерируется, пример построить нельзя";
}

/**
 * Синтаксис, который генератор не разбирает: исключение с позицией.
 */
TEST(Sampler, ParseErrors) {
    std::array<char const *, 4> const bad{"(abc", "[a-z", "a{2", "a)"};
    for (auto const *pattern : bad) {
        EXPECT_THROW(Sampler{pattern}, SamplerError) << "паттерн " << pattern;
    }
}
