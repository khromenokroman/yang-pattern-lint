#pragma once
#include <bitset>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>

/**
 * @brief Набор символов печатного ASCII (0x20..0x7E), из которых строятся примеры.
 *
 * Перевод строки и прочие управляющие символы не используются: их нельзя ввести в CLI.
 */
using CharSet = std::bitset<128>;

/**
 * @brief Все символы, из которых генерируются строки.
 */
[[nodiscard]] CharSet const &printable_ascii();

/**
 * @brief Регулярка XSD не разобрана генератором (не поддерживаемая конструкция).
 */
class SamplerError : public std::runtime_error {
   public:
    using std::runtime_error::runtime_error;
};

/**
 * @brief Генератор строк по регулярке XSD.
 *
 * Разбирает регулярку в дерево и выдаёт случайные строки, которые по замыслу ей подходят.
 * Символы вне печатного ASCII (например, \p{L} за пределами латиницы) не генерируются,
 * классы Unicode приближаются их ASCII-частью. Итог всегда проверяется libxml2.
 */
class Sampler {
   public:
    /// Узел дерева разбора, определён в sampler.cpp
    struct Node;

    /**
     * @param xsd Регулярка XSD.
     * @throw SamplerError Синтаксис, который генератор не понимает.
     */
    explicit Sampler(std::string const &xsd);
    ~Sampler();
    Sampler(Sampler &&) noexcept;
    Sampler &operator=(Sampler &&) noexcept;

    /**
     * @brief Случайная строка по регулярке.
     * @param rng Источник случайности.
     * @return Строка, или пустая, если пример из ASCII построить нельзя.
     */
    [[nodiscard]] std::string sample(std::mt19937 &rng) const;

   private:
    std::unique_ptr<Node> m_root;
};
