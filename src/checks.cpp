#include "checks.hpp"

#include "posix.hpp"
#include "xsd.hpp"

std::vector<Diagnostic> check_pattern(std::string const &file, Pattern const &pattern) {
    std::vector<Diagnostic> out;
    try {
        XsdRegex const regex{pattern.value};
    } catch (XsdError const &e) {
        out.push_back({file, "xsd", std::string("паттерн не компилируется по XSD (libxml2): ") + e.what(), pattern.line, pattern.column});
        return out;
    }
    try {
        PosixRegex const regex{pattern.value};
    } catch (PosixError const &e) {
        out.push_back({file, "posix", std::string("CLI clixon не сможет проверять значения: ") + e.what(), pattern.line, pattern.column});
    }
    return out;
}
