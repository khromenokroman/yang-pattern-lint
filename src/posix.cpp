#include "posix.hpp"

#include <regex.h>

#include <array>
#include <cstdlib>

// Заголовки clixon и cligen рассчитаны на C, объявляем только нужные функции.
extern "C" {
int regexp_xsd2posix(char const *xsd, char **posix);
int cligen_regex_posix_compile(char const *regexp, void **recomp);
int cligen_regex_posix_exec(void *recomp, char const *string);
int cligen_regex_posix_free(void *recomp);
}

namespace {

/**
 * @brief Причина отказа regcomp: cligen её не возвращает, поэтому компилируем ещё раз только ради текста.
 */
std::string regcomp_reason(std::string const &posix) {
    regex_t re{};
    int const err = regcomp(&re, posix.c_str(), REG_EXTENDED | REG_NOSUB);
    if (err == 0) {
        regfree(&re);
        return "regcomp отклонил строку после добавления якорей ^(...)$";
    }
    std::array<char, 256> buf{};
    regerror(err, &re, buf.data(), buf.size());
    return buf.data();
}

} // namespace

std::string xsd_to_posix(std::string const &xsd) {
    char *posix = nullptr;
    if (regexp_xsd2posix(xsd.c_str(), &posix) < 0 || posix == nullptr) {
        std::free(posix);
        throw PosixError("clixon regexp_xsd2posix не смог перевести паттерн");
    }
    std::string out{posix};
    std::free(posix);
    return out;
}

void PosixRegex::Free::operator()(void *regex) const noexcept { cligen_regex_posix_free(regex); }

PosixRegex::PosixRegex(std::string const &xsd) : m_posix(xsd_to_posix(xsd)) {
    void *regex = nullptr;
    int const rc = cligen_regex_posix_compile(m_posix.c_str(), &regex);
    if (rc != 1 || regex == nullptr) {
        throw PosixError("после перевода clixon в POSIX '" + m_posix + "' не компилируется: " + regcomp_reason(m_posix));
    }
    m_regex.reset(regex);
}

bool PosixRegex::matches(std::string const &value) const {
    int const rc = cligen_regex_posix_exec(m_regex.get(), value.c_str());
    if (rc < 0) {
        throw PosixError("cligen_regex_posix_exec вернул ошибку");
    }
    return rc == 1;
}

std::string const &PosixRegex::posix() const noexcept { return m_posix; }
