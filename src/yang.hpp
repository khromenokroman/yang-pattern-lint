#pragma once
#include <cstddef>
#include <filesystem>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

/**
 * @brief Аргумент pattern из YANG-файла.
 */
struct Pattern {
    std::string value;            ///< значение после разбора кавычек, escape и склейки через '+'
    std::string literal;          ///< аргумент как записан в файле, от первой кавычки до последней
    std::size_t line = 0;         ///< строка начала аргумента, с 1
    std::size_t column = 0;       ///< столбец начала аргумента, с 1
    std::size_t keyword_line = 0; ///< строка слова pattern, с 1 (0: как line)
};

/**
 * @brief Ошибка лексического разбора YANG с позицией.
 */
class ParseError : public std::runtime_error {
   public:
    ParseError(std::string const &message, std::size_t line, std::size_t column);

    [[nodiscard]] std::size_t line() const noexcept;
    [[nodiscard]] std::size_t column() const noexcept;

   private:
    std::size_t m_line;
    std::size_t m_column;
};

/**
 * @brief Находит все pattern в тексте YANG-модуля по правилам строк RFC 7950 (раздел 6.1).
 *
 * Учитываются комментарии, строки в одинарных и двойных кавычках, escape-последовательности
 * \\n \\t \\" \\\\ в двойных кавычках (прочие остаются как есть, как в YANG 1.0), удаление отступов
 * в многострочных двойных кавычках и склейка строк через '+'. Слово pattern внутри строк
 * и комментариев не считается оператором.
 *
 * @param text Содержимое файла.
 * @return Найденные pattern в порядке появления.
 * @throw ParseError Незакрытая строка или комментарий, pattern без аргумента, '+' без строки.
 */
[[nodiscard]] std::vector<Pattern> extract_patterns(std::string_view text);

/**
 * @brief Собирает YANG-файлы: файлы берутся как есть, каталоги обходятся рекурсивно (*.yang).
 * @param paths Файлы и каталоги.
 * @return Пути без повторов, отсортированные.
 * @throw std::filesystem::filesystem_error Путь не существует или недоступен.
 */
[[nodiscard]] std::vector<std::filesystem::path> collect_yang_files(std::span<std::filesystem::path const> paths);
