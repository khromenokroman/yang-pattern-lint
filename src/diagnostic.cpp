#include "diagnostic.hpp"

#include <fmt/format.h>

std::string format_diagnostic(Diagnostic const &d) {
    return fmt::format("{}:{}:{}: {} [{}]: {}", d.file, d.line, d.column, d.severity == Diagnostic::Severity::ERROR ? "ошибка" : "предупреждение",
                       d.rule, d.message);
}
