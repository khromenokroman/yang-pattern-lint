#include "compare.hpp"

#include <cstdint>
#include <random>
#include <set>
#include <vector>

#include "sampler.hpp"

namespace {

/// Попыток генерации подходящих строк на один паттерн
constexpr int SAMPLE_ATTEMPTS = 300;
/// Сколько подходящих строк оставлять
constexpr std::size_t MAX_POSITIVES = 80;
/// Мутаций на каждую подходящую строку
constexpr int MUTATIONS = 8;
/// После стольких отказов libyang (лимит PCRE2) паттерн дальше не сравнивается: каждая попытка дорогая
constexpr int MAX_REFERENCE_FAILURES = 3;

std::uint32_t fnv1a(std::string const &text) {
    std::uint32_t hash = 2166136261U;
    for (unsigned char const c : text) {
        hash ^= c;
        hash *= 16777619U;
    }
    return hash;
}

std::string mutate(std::string value, std::mt19937 &rng) {
    auto const &alphabet = printable_ascii();
    auto random_char = [&]() {
        int c = 0;
        do {
            c = std::uniform_int_distribution<int>(0, 127)(rng);
        } while (!alphabet.test(static_cast<std::size_t>(c)));
        return static_cast<char>(c);
    };
    int const kind = value.empty() ? 1 : std::uniform_int_distribution<int>(0, 2)(rng);
    auto const pos = value.empty() ? 0 : std::uniform_int_distribution<std::size_t>(0, value.size() - 1)(rng);
    switch (kind) {
        case 0:
            value.erase(pos, 1);
            break;
        case 1:
            value.insert(value.begin() + static_cast<std::ptrdiff_t>(pos), random_char());
            break;
        default:
            value[pos] = random_char();
            break;
    }
    return value;
}

/**
 * @brief Короче лучше; при равной длине меньше по алфавиту, чтобы вывод не зависел от порядка.
 */
bool better(std::string const &candidate, std::optional<Mismatch> const &current) {
    if (!current) {
        return true;
    }
    if (candidate.size() != current->value.size()) {
        return candidate.size() < current->value.size();
    }
    return candidate < current->value;
}

} // namespace

Mismatches find_mismatches(std::string const &xsd, YangRegex const &reference, PosixRegex const &posix_regex) {
    Mismatches out;
    // ошибку PCRE2 на отдельной строке не считаем расхождением, запоминаем самый короткий пример
    int failures = 0;
    auto xsd_matches = [&](std::string const &value) -> std::optional<bool> {
        try {
            return reference.matches(value);
        } catch (YangRegexError const &e) {
            ++failures;
            if (!out.reference_failed || value.size() < out.reference_failed->size()) {
                out.reference_failed = value;
                out.reference_error = e.what();
            }
            return std::nullopt;
        }
    };
    std::mt19937 rng{fnv1a(xsd)};
    std::set<std::string> candidates{""};
    for (std::size_t c = 0; c < 128; ++c) {
        if (printable_ascii().test(c)) {
            candidates.emplace(1, static_cast<char>(c));
        }
    }

    std::vector<std::string> positives;
    try {
        Sampler const sampler{xsd};
        std::set<std::string> seen;
        for (int i = 0; i < SAMPLE_ATTEMPTS && positives.size() < MAX_POSITIVES && failures < MAX_REFERENCE_FAILURES; ++i) {
            auto value = sampler.sample(rng);
            if (seen.insert(value).second && xsd_matches(value).value_or(false)) {
                positives.push_back(std::move(value));
            }
        }
    } catch (SamplerError const &) {
        // генератор не понял синтаксис: остаются одиночные символы и пустая строка
    }
    for (auto const &value : positives) {
        candidates.insert(value);
        for (int i = 0; i < MUTATIONS; ++i) {
            candidates.insert(mutate(value, rng));
        }
    }

    for (auto const &value : candidates) {
        if (failures >= MAX_REFERENCE_FAILURES) {
            break;
        }
        auto const xsd_result = xsd_matches(value);
        if (!xsd_result) {
            continue;
        }
        bool const by_xsd = *xsd_result;
        bool const by_cli = posix_regex.matches(value);
        if (by_xsd && !by_cli && better(value, out.rejected_by_cli)) {
            out.rejected_by_cli = Mismatch{value, true};
        } else if (!by_xsd && by_cli && better(value, out.accepted_by_cli)) {
            out.accepted_by_cli = Mismatch{value, false};
        }
    }
    return out;
}
