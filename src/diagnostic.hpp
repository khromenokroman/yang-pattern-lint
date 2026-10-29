#pragma once
#include <cstddef>
#include <string>

/**
 * @brief Находка проверки по одному pattern.
 */
struct Diagnostic {
    enum class Severity { ERROR, WARNING };

    std::string file;       ///< путь к YANG-файлу
    std::string rule;       ///< имя проверки: xsd, posix, ...
    std::string message;    ///< что не так, по-русски
    std::size_t line = 0;   ///< строка аргумента pattern, с 1
    std::size_t column = 0; ///< столбец аргумента pattern, с 1
    Severity severity = Severity::ERROR;
};

/**
 * @brief Строка вида "файл:строка:столбец: ошибка [правило]: сообщение".
 */
[[nodiscard]] std::string format_diagnostic(Diagnostic const &d);
