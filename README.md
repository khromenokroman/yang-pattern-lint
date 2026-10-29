# yang-pattern-lint

Проверяет `pattern` в YANG-моделях: регулярка должна одинаково работать по стандарту XSD
и в CLI [clixon](https://github.com/clicon/clixon).

YANG задаёт `pattern` в синтаксисе регулярных выражений XML Schema (XSD). libyang и libxml2
проверяют значения по XSD, а CLI clixon переводит паттерн в POSIX ERE (`regexp_xsd2posix`)
и проверяет ввод через `regcomp`. Перевод не всегда сохраняет смысл: например, в
`[a-z\[\]]+` внутри квадратных скобок POSIX не понимает `\]`, класс закрывается раньше,
и CLI отклоняет любое значение, хотя модель корректна и проходит валидацию.

> Работа в процессе: готовы разбор YANG, `--dump` и проверки `xsd`, `posix`. Остальные
> проверки добавляются по шагам.

## Проверки

| Правило | Уровень | Что проверяет |
|---|---|---|
| `xsd` | ошибка | паттерн компилируется libxml2 (`xmlRegexpCompile`), как у валидаторов на libxml2 |
| `posix` | ошибка | паттерн переводится clixon (`regexp_xsd2posix`) и компилируется cligen (`cligen_regex_posix_compile`), как в CLI |

Правило `posix` вызывает функции libclixon и libcligen, а не их копию, поэтому поведение совпадает
с CLI той версии clixon, с которой собран линтер. cligen добавляет якоря `^(...)$`, только если
переведённая строка сама не начинается с `^` и не кончается на `$`. Пример корректного по XSD
паттерна, который CLI не скомпилирует: вычитание классов `[a-z-[aeiou]]+`, в POSIX его нет.

libxml2 местами мягче стандарта XSD: например, `a{2,1}` (минимум больше максимума) он принимает.
Правило `xsd` находит паттерны, которые libxml2 не скомпилирует, а не любое отступление от стандарта.

```sh
$ yang-pattern-lint models/
models/example.yang:3:39: ошибка [xsd]: паттерн не компилируется по XSD (libxml2): failed to compile: xmlFAParseAtom: expecting ')'
```

## Разбор YANG

Аргумент `pattern` разбирается по правилам строк RFC 7950 (раздел 6.1): одинарные кавычки
берутся как есть, в двойных раскрываются `\n`, `\t`, `\"`, `\\` (прочие `\x` остаются как есть,
как в YANG 1.0), у многострочных двойных строк снимается отступ до столбца кавычки, части
склеиваются через `+`, в том числе без пробелов (`'a'+"'"+'b'`). Слово `pattern` в строках
и комментариях оператором не считается.

```sh
$ yang-pattern-lint --dump models/
models/example.yang:12:15: [a-z][a-z0-9-]*
```

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
