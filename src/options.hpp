#pragma once
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Параметры запуска из командной строки.
 */
struct Options {
    std::vector<std::filesystem::path> paths; ///< YANG-файлы и каталоги, пустой: текущий каталог
    std::vector<std::string> disabled;        ///< правила из --disable
    std::filesystem::path ignore_file;        ///< --ignore-file, пустой: .yang-pattern-lint-ignore, если есть
    bool dump = false;                        ///< вывести найденные pattern вместо проверки
    bool help = false;                        ///< показать справку
    bool version = false;                     ///< показать версию
};

/**
 * @brief Разбирает аргументы командной строки.
 * @param args Аргументы без имени программы.
 * @return Разобранные параметры.
 * @throw std::invalid_argument Неизвестный ключ или нет значения у ключа.
 */
[[nodiscard]] Options parse_options(std::span<std::string_view const> args);

/**
 * @brief Текст справки по ключам.
 */
[[nodiscard]] std::string usage();
