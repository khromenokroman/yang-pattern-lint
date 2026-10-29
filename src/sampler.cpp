#include "sampler.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <variant>

namespace {

/// Сколько повторений сверх минимума пробует генератор для * + {n,}
constexpr int EXTRA_REPEATS = 3;
/// Предел длины примера: защита от {1000} и вложенных кванторов
constexpr std::size_t MAX_LENGTH = 300;

CharSet make_range(char from, char to) {
    CharSet s;
    for (int c = static_cast<unsigned char>(from); c <= static_cast<unsigned char>(to); ++c) {
        s.set(static_cast<std::size_t>(c));
    }
    return s;
}

CharSet make_chars(std::string_view chars) {
    CharSet s;
    for (char const c : chars) {
        s.set(static_cast<unsigned char>(c));
    }
    return s;
}

CharSet const &letters() {
    static CharSet const s = make_range('a', 'z') | make_range('A', 'Z');
    return s;
}

CharSet const &digits() {
    static CharSet const s = make_range('0', '9');
    return s;
}

/// Пунктуация ASCII по Unicode (категория P)
CharSet const &punctuation() {
    static CharSet const s = make_chars(R"(!"#%&'()*,-./:;?@[\]_{})");
    return s;
}

/// Символы ASCII по Unicode (категория S)
CharSet const &symbols() {
    static CharSet const s = make_chars("$+<=>^`|~");
    return s;
}

/**
 * @brief ASCII-часть категории \p{...}; неизвестная категория приближается всем набором.
 */
CharSet category(std::string_view name) {
    if (name == "L" || name == "IsBasicLatin") {
        return name == "L" ? letters() : printable_ascii();
    }
    if (name == "Lu") {
        return make_range('A', 'Z');
    }
    if (name == "Ll") {
        return make_range('a', 'z');
    }
    if (name == "N" || name == "Nd") {
        return digits();
    }
    if (name.starts_with("P")) {
        return punctuation();
    }
    if (name.starts_with("S")) {
        return symbols();
    }
    if (name.starts_with("Z")) {
        return make_chars(" ");
    }
    if (name.starts_with("C") || name.starts_with("M")) {
        return {};
    }
    return printable_ascii();
}

} // namespace

CharSet const &printable_ascii() {
    static CharSet const s = make_range(' ', '~');
    return s;
}

/**
 * @brief Узел дерева: альтернатива последовательностей, последовательность частей, часть с квантором.
 */
struct Sampler::Node {
    struct Piece {
        std::variant<CharSet, std::unique_ptr<Node>> atom;
        int min = 1;
        int max = 1; ///< -1 без ограничения
    };
    std::vector<std::vector<Piece>> branches;
};

namespace {

using Node = Sampler::Node;

/**
 * @brief Рекурсивный разбор регулярки XSD (XML Schema Part 2, приложение F).
 */
class Parser {
   public:
    explicit Parser(std::string_view text) : m_text(text) {}

    std::unique_ptr<Node> parse() {
        auto root = parse_alternation();
        if (m_pos != m_text.size()) {
            fail("лишняя ')'");
        }
        return root;
    }

   private:
    [[noreturn]] void fail(std::string const &what) const { throw SamplerError(what + " в позиции " + std::to_string(m_pos + 1)); }

    [[nodiscard]] bool at_end() const { return m_pos >= m_text.size(); }

    [[nodiscard]] char peek() const { return m_text[m_pos]; }

    std::unique_ptr<Node> parse_alternation() {
        auto node = std::make_unique<Node>();
        node->branches.emplace_back();
        while (!at_end() && peek() != ')') {
            if (peek() == '|') {
                ++m_pos;
                node->branches.emplace_back();
                continue;
            }
            node->branches.back().push_back(parse_piece());
        }
        return node;
    }

    Node::Piece parse_piece() {
        Node::Piece piece;
        char const c = peek();
        if (c == '(') {
            ++m_pos;
            piece.atom = parse_alternation();
            if (at_end() || peek() != ')') {
                fail("не закрыта '('");
            }
            ++m_pos;
        } else if (c == '[') {
            piece.atom = parse_class();
        } else if (c == '\\') {
            piece.atom = parse_escape();
        } else if (c == '.') {
            ++m_pos;
            piece.atom = printable_ascii();
        } else if (c == '?' || c == '*' || c == '+' || c == '{') {
            fail(std::string("квантор '") + c + "' без атома");
        } else {
            ++m_pos;
            piece.atom = make_chars(std::string_view(&c, 1));
        }
        parse_quantifier(piece);
        return piece;
    }

    void parse_quantifier(Node::Piece &piece) {
        if (at_end()) {
            return;
        }
        switch (peek()) {
            case '?':
                piece.min = 0;
                piece.max = 1;
                ++m_pos;
                return;
            case '*':
                piece.min = 0;
                piece.max = -1;
                ++m_pos;
                return;
            case '+':
                piece.min = 1;
                piece.max = -1;
                ++m_pos;
                return;
            case '{':
                break;
            default:
                return;
        }
        ++m_pos;
        piece.min = parse_number();
        piece.max = piece.min;
        if (!at_end() && peek() == ',') {
            ++m_pos;
            piece.max = !at_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0 ? parse_number() : -1;
        }
        if (at_end() || peek() != '}') {
            fail("не закрыт квантор '{'");
        }
        ++m_pos;
    }

    int parse_number() {
        auto const start = m_pos;
        while (!at_end() && std::isdigit(static_cast<unsigned char>(peek())) != 0) {
            ++m_pos;
        }
        if (start == m_pos || m_pos - start > 6) {
            fail("ожидалось число в кванторе");
        }
        return std::stoi(std::string(m_text.substr(start, m_pos - start)));
    }

    /**
     * @brief Escape вне или внутри класса: возвращает набор символов.
     */
    CharSet parse_escape() {
        ++m_pos; // '\'
        if (at_end()) {
            fail("'\\' в конце");
        }
        char const e = peek();
        ++m_pos;
        switch (e) {
            case 'd':
                return digits();
            case 'D':
                return printable_ascii() & ~digits();
            case 's':
                return make_chars(" ");
            case 'S':
                return printable_ascii() & ~make_chars(" ");
            case 'i':
                return letters() | make_chars("_:");
            case 'I':
                return printable_ascii() & ~(letters() | make_chars("_:"));
            case 'c':
                return letters() | digits() | make_chars("._:-");
            case 'C':
                return printable_ascii() & ~(letters() | digits() | make_chars("._:-"));
            case 'w':
                return printable_ascii() & ~(punctuation() | make_chars(" "));
            case 'W':
                return punctuation() | make_chars(" ");
            case 'p':
            case 'P': {
                if (at_end() || peek() != '{') {
                    fail("ожидалась '{' после \\p");
                }
                auto const close = m_text.find('}', m_pos);
                if (close == std::string_view::npos) {
                    fail("не закрыта '{' в \\p");
                }
                auto const set = category(m_text.substr(m_pos + 1, close - m_pos - 1));
                m_pos = close + 1;
                return e == 'p' ? set : printable_ascii() & ~set;
            }
            case 'n':
            case 'r':
            case 't':
                return {}; // управляющие символы не генерируем
            default:
                return make_chars(std::string_view(&e, 1));
        }
    }

    /**
     * @brief Одиночный символ внутри класса или его escape.
     * @return Код символа, или -1 если escape обозначает набор.
     */
    int class_char(CharSet &set_escape) {
        if (peek() == '\\') {
            auto const save = m_pos;
            ++m_pos;
            if (!at_end() && std::string_view("nrt\\|.?*+(){}-[]^").find(peek()) != std::string_view::npos) {
                char const e = peek();
                ++m_pos;
                return e == 'n' ? '\n' : e == 'r' ? '\r' : e == 't' ? '\t' : static_cast<unsigned char>(e);
            }
            m_pos = save;
            set_escape = parse_escape();
            return -1;
        }
        return static_cast<unsigned char>(m_text[m_pos++]);
    }

    CharSet parse_class() {
        ++m_pos; // '['
        bool negated = false;
        if (!at_end() && peek() == '^') {
            negated = true;
            ++m_pos;
        }
        CharSet set;
        bool first = true;
        while (true) {
            if (at_end()) {
                fail("не закрыт класс '['");
            }
            if (peek() == ']' && !first) {
                ++m_pos;
                break;
            }
            if (peek() == '-' && m_pos + 1 < m_text.size() && m_text[m_pos + 1] == '[') {
                ++m_pos;
                auto const subtract = parse_class();
                if (at_end() || peek() != ']') {
                    fail("после вычитания классов ожидалась ']'");
                }
                ++m_pos;
                set &= ~subtract;
                break;
            }
            first = false;
            CharSet escape;
            int const from = class_char(escape);
            if (from < 0) {
                set |= escape;
                continue;
            }
            if (!at_end() && peek() == '-' && m_pos + 1 < m_text.size() && m_text[m_pos + 1] != ']' && m_text[m_pos + 1] != '[') {
                ++m_pos;
                CharSet unused;
                int const to = class_char(unused);
                if (to < 0 || to < from) {
                    fail("неверный диапазон в классе");
                }
                for (int c = from; c <= to && c < 128; ++c) {
                    set.set(static_cast<std::size_t>(c));
                }
            } else if (from < 128) {
                set.set(static_cast<std::size_t>(from));
            }
        }
        set &= printable_ascii();
        return negated ? printable_ascii() & ~set : set;
    }

    std::string_view m_text;
    std::size_t m_pos = 0;
};

char pick(CharSet const &set, std::mt19937 &rng) {
    auto const count = static_cast<int>(set.count());
    int index = std::uniform_int_distribution<int>(0, count - 1)(rng);
    for (std::size_t c = 0; c < set.size(); ++c) {
        if (set.test(c) && index-- == 0) {
            return static_cast<char>(c);
        }
    }
    return ' ';
}

/**
 * @return false, если пример построить нельзя (пустой набор символов)
 */
bool generate(Node const &node, std::mt19937 &rng, std::string &out) {
    auto const &branch = node.branches[std::uniform_int_distribution<std::size_t>(0, node.branches.size() - 1)(rng)];
    for (auto const &piece : branch) {
        int const hi = piece.max < 0 ? piece.min + EXTRA_REPEATS : std::min(piece.max, piece.min + EXTRA_REPEATS);
        // иногда берём точную верхнюю границу, чтобы проверить {n,m} на краю
        int count = std::uniform_int_distribution<int>(piece.min, std::max(piece.min, hi))(rng);
        if (piece.max >= 0 && piece.max - piece.min <= 64 && std::uniform_int_distribution<int>(0, 3)(rng) == 0) {
            count = piece.max;
        }
        for (int i = 0; i < count; ++i) {
            if (out.size() > MAX_LENGTH) {
                return true;
            }
            if (auto const *set = std::get_if<CharSet>(&piece.atom)) {
                if (set->none()) {
                    return false;
                }
                out += pick(*set, rng);
            } else if (!generate(*std::get<std::unique_ptr<Node>>(piece.atom), rng, out)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace

Sampler::Sampler(std::string const &xsd) : m_root(Parser{xsd}.parse()) {}

Sampler::~Sampler() = default;
Sampler::Sampler(Sampler &&) noexcept = default;
Sampler &Sampler::operator=(Sampler &&) noexcept = default;

std::string Sampler::sample(std::mt19937 &rng) const {
    std::string out;
    if (!generate(*m_root, rng, out)) {
        return {};
    }
    return out;
}
