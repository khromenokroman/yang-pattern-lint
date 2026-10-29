#include "yang.hpp"

#include <algorithm>
#include <optional>

ParseError::ParseError(std::string const &message, std::size_t line, std::size_t column)
    : std::runtime_error(message), m_line(line), m_column(column) {}

std::size_t ParseError::line() const noexcept { return m_line; }

std::size_t ParseError::column() const noexcept { return m_column; }

namespace {

/**
 * @brief Лексер YANG: выдаёт слова и строки, пропуская пробелы и комментарии.
 */
class Lexer {
   public:
    /**
     * @brief Лексема: слово без кавычек, строка в кавычках, '+' или разделитель ; { }.
     */
    struct Token {
        enum class Kind { WORD, STRING, PLUS, SEPARATOR, END };
        std::string value;      ///< значение (для STRING после разбора кавычек)
        std::string_view raw;   ///< как записано в тексте
        std::size_t offset = 0; ///< смещение начала
        std::size_t line = 0;
        std::size_t column = 0;
        Kind kind = Kind::END;
    };

    explicit Lexer(std::string_view text) : m_text(text) {}

    Token next() {
        skip_space_and_comments();
        Token token;
        token.offset = m_pos;
        token.line = m_line;
        token.column = column();
        if (m_pos >= m_text.size()) {
            return token;
        }
        char const c = m_text[m_pos];
        if (c == ';' || c == '{' || c == '}') {
            token.kind = Token::Kind::SEPARATOR;
            token.value = std::string(1, c);
            advance();
        } else if (c == '"') {
            token.kind = Token::Kind::STRING;
            token.value = read_double_quoted(token);
        } else if (c == '\'') {
            token.kind = Token::Kind::STRING;
            token.value = read_single_quoted(token);
        } else if (c == '+' && plus_is_operator()) {
            // склейка допускается без пробелов: 'a'+"b" (RFC 7950, optsep)
            token.kind = Token::Kind::PLUS;
            token.value = "+";
            advance();
        } else {
            auto const start = m_pos;
            while (m_pos < m_text.size() && !is_space(m_text[m_pos]) && !is_separator(m_text[m_pos]) && !starts_comment()) {
                advance();
            }
            token.value = std::string(m_text.substr(start, m_pos - start));
            token.kind = Token::Kind::WORD;
        }
        token.raw = m_text.substr(token.offset, m_pos - token.offset);
        return token;
    }

   private:
    static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }

    static bool is_separator(char c) { return c == ';' || c == '{' || c == '}'; }

    [[nodiscard]] bool starts_comment() const {
        return m_pos + 1 < m_text.size() && m_text[m_pos] == '/' && (m_text[m_pos + 1] == '/' || m_text[m_pos + 1] == '*');
    }

    /**
     * @brief '+' в текущей позиции стоит отдельно: дальше кавычка, пробел, комментарий или конец текста.
     */
    [[nodiscard]] bool plus_is_operator() const {
        auto const next = m_pos + 1;
        if (next >= m_text.size()) {
            return true;
        }
        char const n = m_text[next];
        return n == '\'' || n == '"' || is_space(n) || m_text.substr(next).starts_with("//") || m_text.substr(next).starts_with("/*");
    }

    [[nodiscard]] std::size_t column() const { return m_pos - m_line_start + 1; }

    void advance() {
        if (m_text[m_pos] == '\n') {
            ++m_line;
            m_line_start = m_pos + 1;
        }
        ++m_pos;
    }

    void skip_space_and_comments() {
        while (m_pos < m_text.size()) {
            if (is_space(m_text[m_pos])) {
                advance();
            } else if (m_text.substr(m_pos).starts_with("//")) {
                while (m_pos < m_text.size() && m_text[m_pos] != '\n') {
                    advance();
                }
            } else if (m_text.substr(m_pos).starts_with("/*")) {
                auto const line = m_line;
                auto const col = column();
                auto const end = m_text.find("*/", m_pos + 2);
                if (end == std::string_view::npos) {
                    throw ParseError("незакрытый комментарий /*", line, col);
                }
                while (m_pos < end + 2) {
                    advance();
                }
            } else {
                return;
            }
        }
    }

    std::string read_single_quoted(Token const &token) {
        advance();
        auto const start = m_pos;
        auto const end = m_text.find('\'', start);
        if (end == std::string_view::npos) {
            throw ParseError("незакрытая строка в одинарных кавычках", token.line, token.column);
        }
        while (m_pos <= end) {
            advance();
        }
        return std::string(m_text.substr(start, end - start));
    }

    /**
     * @brief Строка в двойных кавычках: escape и удаление отступов по RFC 7950, раздел 6.1.3.
     */
    std::string read_double_quoted(Token const &token) {
        auto const indent = token.column; // столбец после кавычки минус 1
        advance();
        std::string out;
        while (true) {
            if (m_pos >= m_text.size()) {
                throw ParseError("незакрытая строка в двойных кавычках", token.line, token.column);
            }
            char const c = m_text[m_pos];
            if (c == '"') {
                advance();
                return out;
            }
            if (c == '\\') {
                if (m_pos + 1 >= m_text.size()) {
                    throw ParseError("незакрытая строка в двойных кавычках", token.line, token.column);
                }
                char const e = m_text[m_pos + 1];
                switch (e) {
                    case 'n':
                        out += '\n';
                        break;
                    case 't':
                        out += '\t';
                        break;
                    case '"':
                        out += '"';
                        break;
                    case '\\':
                        out += '\\';
                        break;
                    default:
                        // YANG 1.0 оставляет неизвестный escape как есть, YANG 1.1 считает ошибкой
                        out += '\\';
                        out += e;
                        break;
                }
                advance();
                advance();
                continue;
            }
            if (c == '\n') {
                while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) {
                    out.pop_back();
                }
                out += '\n';
                advance();
                // отступ следующей строки снимается до столбца после открывающей кавычки
                std::size_t skipped = 0;
                while (m_pos < m_text.size() && skipped < indent) {
                    if (m_text[m_pos] == ' ') {
                        ++skipped;
                    } else if (m_text[m_pos] == '\t') {
                        skipped += 8;
                    } else {
                        break;
                    }
                    advance();
                }
                continue;
            }
            out += c;
            advance();
        }
    }

    std::string_view m_text;
    std::size_t m_pos = 0;
    std::size_t m_line = 1;
    std::size_t m_line_start = 0;
};

using Token = Lexer::Token;

/**
 * @brief Читает аргумент оператора: слово, строку или склейку строк через '+'.
 * @return Значение, начало и конец аргумента в тексте; nullopt, если аргумента нет.
 */
std::optional<Pattern> read_argument(Lexer &lexer, Token &token, std::string_view text) {
    if (token.kind == Token::Kind::WORD) {
        Pattern p{token.value, std::string(token.raw), token.line, token.column};
        token = lexer.next();
        return p;
    }
    if (token.kind != Token::Kind::STRING) {
        return std::nullopt;
    }
    Pattern p{token.value, {}, token.line, token.column};
    auto const begin = token.offset;
    auto end = token.offset + token.raw.size();
    token = lexer.next();
    while (token.kind == Token::Kind::PLUS) {
        auto const plus = token;
        token = lexer.next();
        if (token.kind != Token::Kind::STRING) {
            throw ParseError("после '+' ожидается строка в кавычках", plus.line, plus.column);
        }
        p.value += token.value;
        end = token.offset + token.raw.size();
        token = lexer.next();
    }
    p.literal = std::string(text.substr(begin, end - begin));
    return p;
}

} // namespace

std::vector<Pattern> extract_patterns(std::string_view text) {
    std::vector<Pattern> patterns;
    Lexer lexer{text};
    auto token = lexer.next();
    // ожидаем ключевое слово оператора: в начале файла и после ; { }
    bool expect_keyword = true;
    while (token.kind != Token::Kind::END) {
        if (token.kind == Token::Kind::SEPARATOR) {
            expect_keyword = true;
            token = lexer.next();
            continue;
        }
        if (expect_keyword && token.kind == Token::Kind::WORD && token.value == "pattern") {
            auto const keyword = token;
            token = lexer.next();
            auto pattern = read_argument(lexer, token, text);
            if (!pattern) {
                throw ParseError("у pattern нет аргумента", keyword.line, keyword.column);
            }
            pattern->keyword_line = keyword.line;
            patterns.push_back(std::move(*pattern));
            expect_keyword = false;
            continue;
        }
        expect_keyword = false;
        token = lexer.next();
    }
    return patterns;
}

std::vector<std::filesystem::path> collect_yang_files(std::span<std::filesystem::path const> paths) {
    std::vector<std::filesystem::path> files;
    for (auto const &path : paths) {
        if (std::filesystem::is_directory(path)) {
            for (auto const &entry : std::filesystem::recursive_directory_iterator(path)) {
                if (entry.is_regular_file() && entry.path().extension() == ".yang") {
                    files.push_back(entry.path().lexically_normal());
                }
            }
        } else if (std::filesystem::exists(path)) {
            files.push_back(path.lexically_normal());
        } else {
            throw std::filesystem::filesystem_error("нет такого файла или каталога", path,
                                                    std::make_error_code(std::errc::no_such_file_or_directory));
        }
    }
    std::ranges::sort(files);
    auto const [first, last] = std::ranges::unique(files);
    files.erase(first, last);
    return files;
}
