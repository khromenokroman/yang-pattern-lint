#include <libxml/xmlversion.h>

#include <cstdlib>
#include <exception>
#include <iostream>
#include <string_view>
#include <vector>

#include "options.hpp"

namespace {

constexpr int EXIT_USAGE = 2;

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
    return EXIT_SUCCESS;
}
