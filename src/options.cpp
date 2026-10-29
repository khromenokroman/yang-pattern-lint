#include "options.hpp"

#include <stdexcept>

Options parse_options(std::span<std::string_view const> args) {
    Options options;
    bool only_paths = false;
    for (auto const arg : args) {
        if (only_paths || !arg.starts_with('-') || arg == "-") {
            options.paths.emplace_back(arg);
        } else if (arg == "--") {
            only_paths = true;
        } else if (arg == "-h" || arg == "--help") {
            options.help = true;
        } else if (arg == "-V" || arg == "--version") {
            options.version = true;
        } else if (arg == "--dump") {
            options.dump = true;
        } else {
            throw std::invalid_argument("неизвестный ключ " + std::string{arg});
        }
    }
    if (options.paths.empty()) {
        options.paths.emplace_back(".");
    }
    return options;
}

std::string usage() {
    return "Использование: yang-pattern-lint [ключи] [ФАЙЛ.yang | КАТАЛОГ]...\n"
           "\n"
           "Проверяет pattern в YANG-файлах: регулярка должна компилироваться по XSD (libxml2)\n"
           "и после перевода clixon в POSIX принимать те же строки, что и по XSD.\n"
           "Каталоги обходятся рекурсивно, берутся файлы *.yang. Без путей: текущий каталог.\n"
           "\n"
           "Ключи:\n"
           "  --dump          вывести найденные pattern с файлом и строкой, без проверки\n"
           "  -h, --help      показать эту справку\n"
           "  -V, --version   показать версию\n"
           "\n"
           "Код возврата: 0 нет ошибок, 1 найдены ошибки, 2 неверные аргументы.\n";
}
