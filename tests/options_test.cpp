#include "options.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <stdexcept>
#include <string_view>

/**
 * Без аргументов проверяется текущий каталог.
 */
TEST(Options, DefaultPathIsCurrentDir) {
    auto const options = parse_options({});
    ASSERT_EQ(options.paths.size(), 1U) << "без аргументов ожидается один путь";
    EXPECT_EQ(options.paths.front(), ".") << "путь по умолчанию";
    EXPECT_FALSE(options.dump);
}

/**
 * Пути и ключи в любом порядке, после -- всё считается путями.
 */
TEST(Options, PathsAndKeys) {
    std::array<std::string_view, 5> const args{"models", "--dump", "a.yang", "--", "--strange.yang"};
    auto const options = parse_options(args);
    EXPECT_TRUE(options.dump) << "ключ --dump";
    ASSERT_EQ(options.paths.size(), 3U) << "аргументы: models --dump a.yang -- --strange.yang";
    EXPECT_EQ(options.paths[0], "models");
    EXPECT_EQ(options.paths[1], "a.yang");
    EXPECT_EQ(options.paths[2], "--strange.yang") << "после -- ключ считается путём";
}

/**
 * Короткие и длинные формы help и version.
 */
TEST(Options, HelpAndVersion) {
    std::array<std::string_view, 2> const short_args{"-h", "-V"};
    auto const short_options = parse_options(short_args);
    EXPECT_TRUE(short_options.help);
    EXPECT_TRUE(short_options.version);

    std::array<std::string_view, 2> const long_args{"--help", "--version"};
    auto const long_options = parse_options(long_args);
    EXPECT_TRUE(long_options.help);
    EXPECT_TRUE(long_options.version);
}

/**
 * Неизвестный ключ отклоняется с его именем в тексте ошибки.
 */
TEST(Options, UnknownKey) {
    std::array<std::string_view, 1> const args{"--strict"};
    try {
        (void)parse_options(args);
        FAIL() << "ожидалось исключение для --strict";
    } catch (std::invalid_argument const &e) {
        EXPECT_THAT(e.what(), testing::HasSubstr("--strict")) << "в ошибке должно быть имя ключа";
    }
}

/**
 * --disable можно повторять, у --ignore-file обязательное значение.
 */
TEST(Options, DisableAndIgnoreFile) {
    std::array<std::string_view, 6> const args{"--disable", "anchors,escape", "--disable", "mismatch", "--ignore-file", "x.ignore"};
    auto const options = parse_options(args);
    EXPECT_THAT(options.disabled, testing::ElementsAre("anchors,escape", "mismatch"));
    EXPECT_EQ(options.ignore_file, "x.ignore");

    std::array<std::string_view, 1> const missing{"--ignore-file"};
    EXPECT_THROW((void)parse_options(missing), std::invalid_argument) << "--ignore-file без значения";
}
