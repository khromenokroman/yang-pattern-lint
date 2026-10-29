#pragma once
#include <memory>
#include <stdexcept>
#include <string>

/**
 * @brief Регулярка XSD не компилируется libxml2.
 */
class XsdError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Регулярное выражение XML Schema, скомпилированное libxml2 (xmlRegexpCompile).
 *
 * Так паттерн проверяют валидаторы на libxml2. Сопоставление всегда по всей строке:
 * в XSD якоря подразумеваются, ^ и $ обычные символы.
 */
class XsdRegex {
   public:
    /**
     * @brief Компилирует паттерн.
     * @param pattern Регулярка в синтаксисе XSD, UTF-8.
     * @throw XsdError libxml2 отклонил паттерн, в тексте сообщение libxml2.
     */
    explicit XsdRegex(std::string const &pattern);

    /**
     * @brief Строка целиком подходит под паттерн.
     * @throw XsdError Ошибка выполнения в libxml2.
     */
    [[nodiscard]] bool matches(std::string const &value) const;

   private:
    struct Free {
        void operator()(void *regexp) const noexcept;
    };
    std::unique_ptr<void, Free> m_regexp;
};
