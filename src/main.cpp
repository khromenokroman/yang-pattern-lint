#include <libxml/xmlversion.h>

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "checks.hpp"
#include "diagnostic.hpp"
#include "options.hpp"
#include "suppress.hpp"
#include "yang.hpp"

namespace {

constexpr int EXIT_FINDINGS = 1;
constexpr int EXIT_USAGE = 2;

std::string read_file(std::filesystem::path const &path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) {
        throw std::runtime_error("не удалось открыть " + path.string());
    }
    return {std::istreambuf_iterator<char>{in}, std::istreambuf_iterator<char>{}};
}

/**
 * @brief Значение в одну строку: переводы строк и табуляции видны как \n и \t.
 */
std::string printable(std::string_view value) {
    std::string out;
    for (char const c : value) {
        if (c == '\n') {
            out += "\\n";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

} // namespace

int main(int argc, char **argv) {
    std::vector<std::string_view> const args(argv + 1, argv + argc);
    Options options;
    try {
        options = parse_options(args);
    } catch (std::exception const &e) {
        std::cerr << "yang-pattern-lint: " << e.what() << "\n\n" << usage();
        return EXIT_USAGE;
    }
    if (options.help) {
        std::cout << usage();
        return EXIT_SUCCESS;
    }
    if (options.version) {
        std::cout << "yang-pattern-lint " << YANG_PATTERN_LINT_VERSION << " (libxml2 " << LIBXML_DOTTED_VERSION << ")\n";
        return EXIT_SUCCESS;
    }

    std::vector<std::filesystem::path> files;
    Suppressions suppressions;
    try {
        files = collect_yang_files(options.paths);
        for (auto const &rules : options.disabled) {
            suppressions.disabled.merge(split_rules(rules));
        }
        auto ignore_file = options.ignore_file;
        if (ignore_file.empty() && std::filesystem::exists(DEFAULT_IGNORE_FILE)) {
            ignore_file = DEFAULT_IGNORE_FILE;
        }
        if (!ignore_file.empty()) {
            suppressions.entries = parse_ignore_file(read_file(ignore_file));
        }
    } catch (std::exception const &e) {
        std::cerr << "yang-pattern-lint: " << e.what() << '\n';
        return EXIT_USAGE;
    }

    int errors = 0;
    for (auto const &file : files) {
        std::vector<Pattern> patterns;
        std::string text;
        try {
            text = read_file(file);
            patterns = extract_patterns(text);
        } catch (ParseError const &e) {
            std::cout << file.string() << ':' << e.line() << ':' << e.column() << ": ошибка разбора: " << e.what() << '\n';
            ++errors;
            continue;
        } catch (std::exception const &e) {
            std::cout << file.string() << ": " << e.what() << '\n';
            ++errors;
            continue;
        }
        std::vector<std::string_view> lines;
        for (std::string_view rest = text; !rest.empty();) {
            auto const end = rest.find('\n');
            lines.push_back(rest.substr(0, end));
            rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
        }
        for (auto const &p : patterns) {
            if (options.dump) {
                std::cout << file.string() << ':' << p.line << ':' << p.column << ": " << printable(p.value) << '\n';
                continue;
            }
            for (auto const &d : check_pattern(file.string(), p)) {
                if (is_suppressed(suppressions, d, p, lines)) {
                    continue;
                }
                std::cout << format_diagnostic(d) << '\n';
                if (d.severity == Diagnostic::Severity::ERROR) {
                    ++errors;
                }
            }
        }
    }
    return errors > 0 ? EXIT_FINDINGS : EXIT_SUCCESS;
}
