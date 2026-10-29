#include "yang_regex.hpp"

#include <libyang/libyang.h>
#include <libyang/plugins_types.h>
#include <libyang/version.h>

namespace {

/**
 * @brief Текст последней ошибки libyang: в версии 3 он глобальный, в версии 2 у контекста.
 */
std::string last_error([[maybe_unused]] ly_ctx const *ctx) {
#if LY_VERSION_MAJOR >= 3
    char const *msg = ly_last_logmsg();
#else
    char const *msg = ly_errmsg(ctx);
#endif
    return msg != nullptr ? msg : "libyang не сообщила причину";
}

/**
 * @brief Модуль из одного leaf с паттерном в двойных кавычках (с экранированием \\ и \").
 */
std::string module_text(std::string const &xsd) {
    std::string escaped;
    for (char const c : xsd) {
        if (c == '\\' || c == '"') {
            escaped += '\\';
        }
        escaped += c;
    }
    return "module m { yang-version 1.1; namespace \"urn:yang-pattern-lint\"; prefix m;\n"
           "  leaf v { type string { pattern \"" +
           escaped + "\"; } }\n}\n";
}

} // namespace

struct YangRegex::Impl {
    struct FreeCtx {
        void operator()(ly_ctx *ctx) const noexcept { ly_ctx_destroy(ctx); }
    };
    std::unique_ptr<ly_ctx, FreeCtx> ctx;
    lysc_pattern **patterns = nullptr;
};

YangRegex::YangRegex(std::string const &xsd) : m_impl(std::make_unique<Impl>()) {
    // сообщения libyang не печатаем, только сохраняем последнее
    ly_log_options(LY_LOSTORE_LAST);
    ly_ctx *ctx = nullptr;
    if (ly_ctx_new(nullptr, LY_CTX_NO_YANGLIBRARY, &ctx) != LY_SUCCESS) {
        throw YangRegexError("не удалось создать контекст libyang");
    }
    m_impl->ctx.reset(ctx);
    lys_module *module = nullptr;
    if (lys_parse_mem(ctx, module_text(xsd).c_str(), LYS_IN_YANG, &module) != LY_SUCCESS || module == nullptr || module->compiled == nullptr ||
        module->compiled->data == nullptr) {
        throw YangRegexError(last_error(ctx));
    }
    auto const *leaf = reinterpret_cast<lysc_node_leaf const *>(module->compiled->data);
    if (leaf->type == nullptr || leaf->type->basetype != LY_TYPE_STRING) {
        throw YangRegexError("в скомпилированном модуле нет строкового leaf");
    }
    m_impl->patterns = reinterpret_cast<lysc_type_str const *>(leaf->type)->patterns;
}

YangRegex::~YangRegex() = default;

bool YangRegex::matches(std::string const &value) const {
    ly_err_item *err = nullptr;
    LY_ERR const rc = lyplg_type_validate_patterns(m_impl->patterns, value.c_str(), value.size(), &err);
    std::string message;
    if (err != nullptr) {
        message = err->msg != nullptr ? err->msg : "";
        ly_err_free(err);
    }
    if (rc == LY_SUCCESS) {
        return true;
    }
    if (rc == LY_EVALID) {
        return false;
    }
    throw YangRegexError(message.empty() ? "ошибка PCRE2 при проверке строки" : message);
}
