#include "yang_regex.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "xsd.hpp"

namespace {

/// Паттерн ipv6-address-no-zone из RFC 6991 (ietf-inet-types), первая часть
constexpr char const IPV6[] = R"(((:|[0-9a-fA-F]{0,4}):)([0-9a-fA-F]{0,4}:){0,5}((([0-9a-fA-F]{0,4}:)?(:|[0-9a-fA-F]{0,4}))|)"
                              R"((((25[0-5]|2[0-4][0-9]|[01]?[0-9]?[0-9])\.){3}(25[0-5]|2[0-4][0-9]|[01]?[0-9]?[0-9]))))";

} // namespace

/**
 * Сопоставление по всей строке, ^ и $ обычные символы, как положено в XSD.
 */
TEST(YangRegex, MatchesLikeXsd) {
    YangRegex const regex{"[a-z]+(-[0-9]+)?"};
    EXPECT_TRUE(regex.matches("abc-12"));
    EXPECT_FALSE(regex.matches("abc-")) << "сопоставление по всей строке";
    YangRegex const anchors{"^a$"};
    EXPECT_TRUE(anchors.matches("^a$")) << "^ и $ в XSD обычные символы";
    EXPECT_FALSE(anchors.matches("a"));
}

/**
 * Некомпилируемый паттерн: исключение с сообщением libyang.
 */
TEST(YangRegex, CompileError) {
    try {
        YangRegex const regex{"(abc"};
        FAIL() << "ожидалась ошибка для (abc";
    } catch (YangRegexError const &e) {
        EXPECT_THAT(std::string(e.what()), testing::Not(testing::IsEmpty()));
    }
}

/**
 * Паттерн с кавычками и обратным слешем переносится в модуль без искажений.
 */
TEST(YangRegex, QuotesAndBackslash) {
    YangRegex const regex{R"(["'\\]+)"};
    EXPECT_TRUE(regex.matches(R"("'\)")) << "кавычки и обратный слеш";
    EXPECT_FALSE(regex.matches("a"));
}

/**
 * На вложенных {m,n} libxml2 принимает лишнее, libyang нет: поэтому эталон libyang.
 * Группа из пяти hex-цифр в IPv6 недопустима.
 */
TEST(YangRegex, StricterThanLibxml2OnNestedCounters) {
    YangRegex const yang{IPV6};
    EXPECT_TRUE(yang.matches("2001:db8::1"));
    EXPECT_FALSE(yang.matches("::0591F")) << "libyang: пять hex-цифр в группе";
    XsdRegex const libxml2{IPV6};
    EXPECT_TRUE(libxml2.matches("::0591F")) << "libxml2 2.9 ошибочно принимает; если исправлено, тест можно убрать";
}
