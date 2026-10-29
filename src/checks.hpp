#pragma once
#include <string>
#include <vector>

#include "diagnostic.hpp"
#include "yang.hpp"

/**
 * @brief Проверки одного pattern.
 * @param file Путь к файлу для диагностики.
 * @param pattern Разобранный аргумент.
 * @return Находки, пустой если всё в порядке.
 */
[[nodiscard]] std::vector<Diagnostic> check_pattern(std::string const &file, Pattern const &pattern);
