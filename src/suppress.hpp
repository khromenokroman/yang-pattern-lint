#pragma once
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "diagnostic.hpp"
#include "yang.hpp"

/**
 * @brief Метка в комментарии YANG, отключающая проверки для одного pattern.
 */
inline constexpr std::string_view DISABLE_MARKER = "yang-pattern-lint-disable";

/**
 * @brief Файл исключений по умолчанию, ищется в текущем каталоге.
 */
inline constexpr char const DEFAULT_IGNORE_FILE[] = ".yang-pattern-lint-ignore";

/**
 * @brief Что не выводить.
 */
struct Suppressions {
    /**
     * @brief Строка файла исключений: glob пути и правила (пустой набор: все правила).
     */
    struct Entry {
        std::string glob;
        std::set<std::string> rules;
    };

    std::set<std::string> disabled; ///< правила, отключённые ключом --disable
    std::vector<Entry> entries;     ///< строки файла исключений
};

/**
 * @brief Разбирает файл исключений.
 *
 * Формат строки: `glob [правило...]`, правила через пробел или запятую; без правил подавляются все.
 * Пустые строки и строки с # в начале пропускаются. Glob сравнивается с путём файла и с каждым
 * его хвостом после '/', поэтому `*.yang` и `models/a.yang` работают при любом каталоге запуска.
 *
 * @param text Содержимое файла.
 */
[[nodiscard]] std::vector<Suppressions::Entry> parse_ignore_file(std::string_view text);

/**
 * @brief Список правил из строки через запятую.
 */
[[nodiscard]] std::set<std::string> split_rules(std::string_view text);

/**
 * @brief Находка подавлена ключом, файлом исключений или меткой в комментарии.
 *
 * Метка ищется в комментарии на строке слова pattern, строке аргумента или строке над словом:
 * `// yang-pattern-lint-disable` (все правила) или `// yang-pattern-lint-disable mismatch anchors`.
 *
 * @param s Ключи и файл исключений.
 * @param d Находка.
 * @param p Паттерн, к которому относится находка.
 * @param lines Строки файла.
 */
[[nodiscard]] bool is_suppressed(Suppressions const &s, Diagnostic const &d, Pattern const &p, std::vector<std::string_view> const &lines);
