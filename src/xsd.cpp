#include "xsd.hpp"

#include <libxml/xmlerror.h>
#include <libxml/xmlregexp.h>

namespace {

/**
 * @brief Перехватывает сообщения libxml2 на время вызова вместо вывода в stderr.
 */
class ErrorCapture {
   public:
    ErrorCapture() { xmlSetStructuredErrorFunc(&m_message, &ErrorCapture::handle); }
    ~ErrorCapture() { xmlSetStructuredErrorFunc(nullptr, nullptr); }
    ErrorCapture(ErrorCapture const &) = delete;
    ErrorCapture &operator=(ErrorCapture const &) = delete;

    [[nodiscard]] std::string message() const {
        auto out = m_message;
        while (!out.empty() && (out.back() == '\n' || out.back() == ' ')) {
            out.pop_back();
        }
        return out.empty() ? "libxml2 не сообщил причину" : out;
    }

   private:
    static void handle(void *data, xmlErrorPtr error) {
        auto *message = static_cast<std::string *>(data);
        if (error != nullptr && error->message != nullptr && message->empty()) {
            *message = error->message;
        }
    }

    std::string m_message;
};

} // namespace

void XsdRegex::Free::operator()(void *regexp) const noexcept { xmlRegFreeRegexp(static_cast<xmlRegexpPtr>(regexp)); }

XsdRegex::XsdRegex(std::string const &pattern) {
    ErrorCapture const capture;
    m_regexp.reset(xmlRegexpCompile(reinterpret_cast<xmlChar const *>(pattern.c_str())));
    if (!m_regexp) {
        throw XsdError(capture.message());
    }
}

bool XsdRegex::matches(std::string const &value) const {
    ErrorCapture const capture;
    int const rc = xmlRegexpExec(static_cast<xmlRegexpPtr>(m_regexp.get()), reinterpret_cast<xmlChar const *>(value.c_str()));
    if (rc < 0) {
        throw XsdError(capture.message());
    }
    return rc == 1;
}
