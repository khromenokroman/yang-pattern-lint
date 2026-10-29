#pragma once
#include <optional>
#include <string>

#include "posix.hpp"
#include "yang_regex.hpp"

/**
 * @brief Строка, которую XSD и CLI проверяют по-разному.
 */
struct Mismatch {
    std::string value; ///< пример строки
    bool xsd = false;  ///< подходит по XSD (эталон libyang)
};

/**
 * @brief Самые короткие расхождения в каждую сторону.
 */
struct Mismatches {
    std::optional<Mismatch> rejected_by_cli;     ///< XSD принимает, CLI отклоняет
    std::optional<Mismatch> accepted_by_cli;     ///< XSD отклоняет, CLI принимает
    std::optional<std::string> reference_failed; ///< строка, которую libyang не смогла проверить
    std::string reference_error;                 ///< текст этой ошибки
};

/**
 * @brief Ищет строки, на которых XSD и CLI clixon расходятся.
 *
 * Эталон смысла паттерна libyang (PCRE2). Кандидаты: пустая строка, все одиночные печатные символы
 * ASCII, строки, построенные генератором по регулярке и принятые libyang, и их мутации (удаление,
 * вставка, замена символа). libxml2 эталоном не служит: на вложенных {m,n} он принимает лишнее.
 * Случайность детерминирована хешем паттерна, результат повторяем.
 *
 * @param xsd Регулярка XSD (для генератора и зерна).
 * @param reference Скомпилированная libyang.
 * @param posix_regex Скомпилированная как в CLI.
 */
[[nodiscard]] Mismatches find_mismatches(std::string const &xsd, YangRegex const &reference, PosixRegex const &posix_regex);
