#include "suppress.hpp"

#include <fnmatch.h>

namespace {

bool is_separator(char c) { return c == ' ' || c == '\t' || c == ','; }

/**
 * @brief Правила после метки до конца строки или конца комментария; пустой набор: все правила.
 * @return false, если метки в комментарии на строке нет.
 */
bool marker_rules(std::string_view line, std::set<std::string> &rules) {
    auto const pos = line.find(DISABLE_MARKER);
    if (pos == std::string_view::npos) {
        return false;
    }
    auto const before = line.substr(0, pos);
    if (before.find("//") == std::string_view::npos && before.find("/*") == std::string_view::npos) {
        return false;
    }
    auto rest = line.substr(pos + DISABLE_MARKER.size());
    if (auto const end = rest.find("*/"); end != std::string_view::npos) {
        rest = rest.substr(0, end);
    }
    rules = split_rules(rest);
    return true;
}

bool glob_matches(std::string const &glob, std::string const &path) {
    if (fnmatch(glob.c_str(), path.c_str(), 0) == 0) {
        return true;
    }
    for (auto slash = path.find('/'); slash != std::string::npos; slash = path.find('/', slash + 1)) {
        if (fnmatch(glob.c_str(), path.c_str() + slash + 1, 0) == 0) {
            return true;
        }
    }
    return false;
}

} // namespace

std::set<std::string> split_rules(std::string_view text) {
    std::set<std::string> rules;
    std::size_t i = 0;
    while (i < text.size()) {
        while (i < text.size() && is_separator(text[i])) {
            ++i;
        }
        auto const start = i;
        while (i < text.size() && !is_separator(text[i])) {
            ++i;
        }
        if (i > start) {
            rules.emplace(text.substr(start, i - start));
        }
    }
    return rules;
}

std::vector<Suppressions::Entry> parse_ignore_file(std::string_view text) {
    std::vector<Suppressions::Entry> entries;
    while (!text.empty()) {
        auto const end = text.find('\n');
        auto line = text.substr(0, end);
        text = end == std::string_view::npos ? std::string_view{} : text.substr(end + 1);
        auto const first = line.find_first_not_of(" \t\r");
        if (first == std::string_view::npos || line[first] == '#') {
            continue;
        }
        line = line.substr(first);
        auto const glob_end = line.find_first_of(" \t\r");
        Suppressions::Entry entry;
        entry.glob = std::string(line.substr(0, glob_end));
        if (glob_end != std::string_view::npos) {
            auto rest = line.substr(glob_end);
            rest.remove_suffix(rest.ends_with('\r') ? 1 : 0);
            entry.rules = split_rules(rest);
        }
        entries.push_back(std::move(entry));
    }
    return entries;
}

bool is_suppressed(Suppressions const &s, Diagnostic const &d, Pattern const &p, std::vector<std::string_view> const &lines) {
    if (s.disabled.contains(d.rule)) {
        return true;
    }
    for (auto const &entry : s.entries) {
        if ((entry.rules.empty() || entry.rules.contains(d.rule)) && glob_matches(entry.glob, d.file)) {
            return true;
        }
    }
    auto const keyword = p.keyword_line != 0 ? p.keyword_line : p.line;
    for (auto const number : {keyword - 1, keyword, p.line}) {
        if (number == 0 || number > lines.size()) {
            continue;
        }
        if (number == keyword - 1) {
            // строка над pattern считается, только если на ней один комментарий
            auto const line = lines[number - 1];
            auto const first = line.find_first_not_of(" \t");
            if (first == std::string_view::npos || !(line.substr(first).starts_with("//") || line.substr(first).starts_with("/*"))) {
                continue;
            }
        }
        std::set<std::string> rules;
        if (marker_rules(lines[number - 1], rules) && (rules.empty() || rules.contains(d.rule))) {
            return true;
        }
    }
    return false;
}
