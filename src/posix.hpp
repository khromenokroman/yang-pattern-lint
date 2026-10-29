#pragma once
#include <memory>
#include <stdexcept>
#include <string>

/**
 * @brief Паттерн не удалось перевести в POSIX или скомпилировать так, как это делает CLI clixon.
 */
class PosixError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Переводит регулярку XSD в POSIX ERE функцией clixon regexp_xsd2posix.
 * @param xsd Регулярка в синтаксисе XSD.
 * @return Регулярка POSIX в том виде, в каком её получает CLI.
 * @throw PosixError clixon вернул ошибку.
 */
[[nodiscard]] std::string xsd_to_posix(std::string const &xsd);

/**
 * @brief Регулярка XSD, переведённая и скомпилированная так же, как в CLI clixon.
 *
 * Перевод regexp_xsd2posix из libclixon, компиляция cligen_regex_posix_compile из libcligen:
 * она добавляет якоря ^(...)$, если переведённая строка сама не начинается с ^ и не кончается на $,
 * и вызывает regcomp(REG_EXTENDED | REG_NOSUB).
 */
class PosixRegex {
   public:
    /**
     * @brief Переводит и компилирует паттерн.
     * @param xsd Регулярка в синтаксисе XSD.
     * @throw PosixError Перевод не удался или regcomp отклонил результат; в тексте переведённая строка.
     */
    explicit PosixRegex(std::string const &xsd);

    /**
     * @brief Строка подходит под паттерн так, как её проверит CLI.
     * @throw PosixError Ошибка выполнения в cligen.
     */
    [[nodiscard]] bool matches(std::string const &value) const;

    /**
     * @brief Переведённая регулярка (до якорей cligen).
     */
    [[nodiscard]] std::string const &posix() const noexcept;

   private:
    struct Free {
        void operator()(void *regex) const noexcept;
    };
    std::string m_posix;
    std::unique_ptr<void, Free> m_regex;
};
