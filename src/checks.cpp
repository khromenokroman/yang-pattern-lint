#include "checks.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <optional>
#include <string_view>

#include "compare.hpp"
#include "posix.hpp"
#include "xsd.hpp"
#include "yang_regex.hpp"

namespace {

/**
 * @brief Строка в кавычках, непечатные символы как \n \t \xNN.
 */
std::string quoted(std::string const &value) {
    std::string out = "'";
    for (unsigned char const c : value) {
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else if (c < 0x20 || c == 0x7f) {
            out += fmt::format("\\x{:02x}", c);
        } else {
            out += static_cast<char>(c);
        }
    }
    return out + "'";
}

/**
 * @brief Предупреждения по записи паттерна, не зависящие от движков.
 */
void style_warnings(std::string const &file, Pattern const &pattern, std::vector<Diagnostic> &out) {
    auto warn = [&](char const *rule, std::string message) {
        out.push_back({file, rule, std::move(message), pattern.line, pattern.column, Diagnostic::Severity::WARNING});
    };
    auto const &v = pattern.value;
    if (v.find("\\\\") != std::string::npos) {
        warn("escaped-backslash",
             "в паттерне \\\\ (буквальный обратный слеш): инструменты, переписывающие кавычки "
             "YANG ('...' в \"...\"), часто превращают его в \\ и меняют смысл регулярки");
    }
    bool const caret = !v.empty() && v.front() == '^';
    bool const dollar = v.size() > 1 && v.back() == '$' && v[v.size() - 2] != '\\';
    if (caret || dollar) {
        warn("anchors", fmt::format("паттерн {}: в XSD ^ и $ обычные символы, а не якоря; паттерн и так сопоставляется "
                                    "со всей строкой, а CLI clixon считает их якорями",
                                    caret && dollar ? "начинается с ^ и кончается на $"
                                    : caret         ? "начинается с ^"
                                                    : "кончается на $"));
    }
    // неизвестный escape в двойных кавычках: в YANG 1.1 ошибка, в YANG 1.0 остаётся как есть
    char quote = 0;
    for (std::size_t i = 0; i < pattern.literal.size(); ++i) {
        char const c = pattern.literal[i];
        if (quote == 0) {
            if (c == '\'' || c == '"') {
                quote = c;
            }
        } else if (c == quote) {
            quote = 0;
        } else if (quote == '"' && c == '\\' && i + 1 < pattern.literal.size()) {
            char const e = pattern.literal[++i];
            if (std::string_view("ntr\"\\").find(e) == std::string_view::npos) {
                warn("escape", fmt::format("\\{} в двойных кавычках: в YANG 1.1 это ошибка, YANG 1.0 оставляет как есть; "
                                           "запишите паттерн в одинарных кавычках или удвойте обратный слеш",
                                           e));
                break;
            }
        }
    }
}

} // namespace

std::vector<Diagnostic> check_pattern(std::string const &file, Pattern const &pattern) {
    std::vector<Diagnostic> out;
    style_warnings(file, pattern, out);
    try {
        XsdRegex const regex{pattern.value};
    } catch (XsdError const &e) {
        out.push_back({file, "xsd", std::string("паттерн не компилируется по XSD (libxml2): ") + e.what(), pattern.line, pattern.column});
        return out;
    }
    std::optional<YangRegex> reference;
    try {
        reference.emplace(pattern.value);
    } catch (YangRegexError const &e) {
        out.push_back({file, "libyang", std::string("паттерн не компилируется в libyang: ") + e.what(), pattern.line, pattern.column});
        return out;
    }
    std::optional<PosixRegex> posix;
    try {
        posix.emplace(pattern.value);
    } catch (PosixError const &e) {
        out.push_back({file, "posix", std::string("CLI clixon не сможет проверять значения: ") + e.what(), pattern.line, pattern.column});
        return out;
    }
    auto const found = find_mismatches(pattern.value, *reference, *posix);
    std::vector<std::string> examples;
    if (found.rejected_by_cli) {
        examples.push_back(fmt::format("{} по XSD подходит, CLI отклоняет", quoted(found.rejected_by_cli->value)));
    }
    if (found.accepted_by_cli) {
        examples.push_back(fmt::format("{} по XSD не подходит, CLI принимает", quoted(found.accepted_by_cli->value)));
    }
    if (!examples.empty()) {
        out.push_back({file, "mismatch",
                       fmt::format("XSD (libyang) и CLI clixon проверяют значения по-разному: {} (POSIX после перевода: {})",
                                   fmt::join(examples, "; "), quoted(posix->posix())),
                       pattern.line, pattern.column});
    }
    if (found.reference_failed) {
        out.push_back({file, "libyang-exec",
                       fmt::format("libyang не смогла проверить строку {}: {}; сравнение с CLI для паттерна остановлено",
                                   quoted(*found.reference_failed), found.reference_error),
                       pattern.line, pattern.column, Diagnostic::Severity::WARNING});
    }
    return out;
}
