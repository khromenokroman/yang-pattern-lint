#pragma once
#include <memory>
#include <stdexcept>
#include <string>

/**
 * @brief libyang не скомпилировала паттерн или не смогла проверить строку.
 */
class YangRegexError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Регулярка XSD, скомпилированная libyang (PCRE2), эталон смысла паттерна.
 *
 * Паттерн компилируется в составе модуля из одного leaf, созданного в памяти, а строки проверяются
 * lyplg_type_validate_patterns: так же, как значение проверяет yanglint.
 */
class YangRegex {
   public:
    /**
     * @param xsd Регулярка XSD.
     * @throw YangRegexError libyang отклонила паттерн, в тексте сообщение libyang.
     */
    explicit YangRegex(std::string const &xsd);
    ~YangRegex();
    YangRegex(YangRegex const &) = delete;
    YangRegex &operator=(YangRegex const &) = delete;

    /**
     * @brief Строка целиком подходит под паттерн.
     * @throw YangRegexError Ошибка PCRE2 при проверке.
     */
    [[nodiscard]] bool matches(std::string const &value) const;

   private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
