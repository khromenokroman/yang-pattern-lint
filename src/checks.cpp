#include "checks.hpp"

#include "xsd.hpp"

std::vector<Diagnostic> check_pattern(std::string const &file, Pattern const &pattern) {
    std::vector<Diagnostic> out;
    try {
        XsdRegex const regex{pattern.value};
    } catch (XsdError const &e) {
        out.push_back({file, "xsd", std::string("паттерн не компилируется по XSD (libxml2): ") + e.what(), pattern.line, pattern.column});
    }
    return out;
}
