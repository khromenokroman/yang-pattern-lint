#include "checks.hpp"

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <optional>

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

} // namespace

std::vector<Diagnostic> check_pattern(std::string const &file, Pattern const &pattern) {
    std::vector<Diagnostic> out;
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
