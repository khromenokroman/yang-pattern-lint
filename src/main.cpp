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

#include "options.hpp"
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
    try {
        files = collect_yang_files(options.paths);
    } catch (std::exception const &e) {
        std::cerr << "yang-pattern-lint: " << e.what() << '\n';
        return EXIT_USAGE;
    }

    int errors = 0;
    for (auto const &file : files) {
        std::vector<Pattern> patterns;
        try {
            patterns = extract_patterns(read_file(file));
        } catch (ParseError const &e) {
            std::cout << file.string() << ':' << e.line() << ':' << e.column() << ": ошибка разбора: " << e.what() << '\n';
            ++errors;
            continue;
        } catch (std::exception const &e) {
            std::cout << file.string() << ": " << e.what() << '\n';
            ++errors;
            continue;
        }
        if (options.dump) {
            for (auto const &p : patterns) {
                std::cout << file.string() << ':' << p.line << ':' << p.column << ": " << printable(p.value) << '\n';
            }
        }
    }
    return errors > 0 ? EXIT_FINDINGS : EXIT_SUCCESS;
}
