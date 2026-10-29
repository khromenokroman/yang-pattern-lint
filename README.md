# yang-pattern-lint

Проверяет `pattern` в YANG-моделях: регулярка должна одинаково работать по стандарту XSD
и в CLI [clixon](https://github.com/clicon/clixon).

YANG задаёт `pattern` в синтаксисе регулярных выражений XML Schema (XSD). libyang и libxml2
проверяют значения по XSD, а CLI clixon переводит паттерн в POSIX ERE (`regexp_xsd2posix`)
и проверяет ввод через `regcomp`. Перевод не всегда сохраняет смысл: например, в
`[a-z\[\]]+` внутри квадратных скобок POSIX не понимает `\]`, класс закрывается раньше,
и CLI отклоняет любое значение, хотя модель корректна и проходит валидацию.

> Работа в процессе: сейчас готов каркас (ключи, сборка, зависимости). Разбор YANG и проверки
> добавляются по шагам.

## Зависимости

- **libclixon** и **libcligen** >= 7.0: используется функция `regexp_xsd2posix`, чтобы перевод
  в POSIX был ровно тем же, что в CLI. В репозиториях Debian их нет, ставятся из исходников:
  ```sh
  for repo in cligen clixon; do git clone --depth 1 --branch 7.9.0 https://github.com/clicon/$repo.git; done
  (cd cligen && ./configure --prefix=/usr && make && sudo make install)
  (cd clixon && ./configure --prefix=/usr --without-restconf && make && sudo make install)
  sudo ldconfig
  ```
- **libxml2**: проверка по XSD (`xmlRegexpCompile`).

## Сборка

```sh
sudo apt install build-essential cmake flex bison libssl-dev libfmt-dev libxml2-dev libgtest-dev libgmock-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

Пакет DEB: `cd build && cpack -G DEB`. Зависимость от libclixon в пакет не прописывается,
clixon должен быть установлен отдельно.

## Использование

```sh
yang-pattern-lint [ключи] [ФАЙЛ.yang | КАТАЛОГ]...
```

| Ключ | Что делает |
|---|---|
| `--dump` | вывести найденные `pattern` с файлом и строкой, без проверки |
| `-h`, `--help` | справка |
| `-V`, `--version` | версия |

Код возврата: 0 нет ошибок, 1 найдены ошибки, 2 неверные аргументы.

## Лицензия

GPL-3.0-or-later, см. [LICENSE](LICENSE).
