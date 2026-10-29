# yang-pattern-lint

Проверяет `pattern` в YANG-моделях: регулярка должна одинаково работать по стандарту XSD
и в CLI [clixon](https://github.com/clicon/clixon).

YANG задаёт `pattern` в синтаксисе регулярных выражений XML Schema (XSD). libyang и libxml2
проверяют значения по XSD, а CLI clixon переводит паттерн в POSIX ERE (`regexp_xsd2posix`)
и проверяет ввод через `regcomp`. Перевод не всегда сохраняет смысл: например, в
`[a-z\[\]]+` внутри квадратных скобок POSIX не понимает `\]`, класс закрывается раньше,
и CLI отклоняет любое значение, хотя модель корректна и проходит валидацию.

## Проверки

| Правило | Уровень | Что проверяет |
|---|---|---|
| `xsd` | ошибка | паттерн компилируется libxml2 (`xmlRegexpCompile`), как у валидаторов на libxml2 |
| `libyang` | ошибка | паттерн компилируется libyang (PCRE2), как у yanglint |
| `posix` | ошибка | паттерн переводится clixon (`regexp_xsd2posix`) и компилируется cligen (`cligen_regex_posix_compile`), как в CLI |
| `mismatch` | ошибка | есть строка, которую XSD (эталон libyang) и CLI проверяют по-разному; выводится самый короткий пример в каждую сторону |
| `libyang-exec` | предупреждение | PCRE2 не смогла проверить строку (лимит перебора), сравнение для паттерна остановлено |
| `escaped-backslash` | предупреждение | в значении `\\` (буквальный обратный слеш): инструменты, переписывающие кавычки (`'...'` в `"..."`), часто превращают его в `\` |
| `anchors` | предупреждение | паттерн начинается с `^` или кончается на `$`: в XSD это символы, а не якоря |
| `escape` | предупреждение | неизвестный escape в двойных кавычках (`"\d"`): ошибка в YANG 1.1, в YANG 1.0 остаётся как есть |

Правило `posix` вызывает функции libclixon и libcligen, а не их копию, поэтому поведение совпадает
с CLI той версии clixon, с которой собран линтер. cligen добавляет якоря `^(...)$`, только если
переведённая строка сама не начинается с `^` и не кончается на `$`. Пример корректного по XSD
паттерна, который CLI не скомпилирует: вычитание классов `[a-z-[aeiou]]+`, в POSIX его нет.

Правило `mismatch` строит строки по самой регулярке (свой разбор XSD: альтернативы, группы, кванторы,
классы с отрицанием и вычитанием, `\d \w \s \i \c \p{..}`), берёт те, что принимает libyang,
добавляет их мутации, пустую строку и все одиночные печатные символы ASCII и сравнивает решения
libyang и CLI. Случайность детерминирована хешем паттерна: вывод повторяется от запуска к запуску.
Эталоном служит libyang, а не libxml2: на вложенных `{m,n}` libxml2 2.9 принимает лишнее
(например, `::0591F` по паттерну IPv6 из RFC 6991).

Найденные так ошибки перевода clixon 7.9 (примеры):

| XSD | после `regexp_xsd2posix` | что ломается |
|---|---|---|
| `[0-9]{4}\-(0[1-9]\|1[0-2])` | `[0-9]{4}(0[1-9-]\|1[0-2])` | `\-` вне класса пропадает, `-` переезжает в следующий класс |
| `[&'*+/=]` | `[&'*/=]` | `+` после `*` в классе теряется |
| `[a-z\[\]]+` | без изменений | в POSIX `\]` закрывает класс |
| `\p{Lu}+` | `a-zA-Z+` | класс теряет скобки |
| `^a$` | без изменений | в XSD `^` и `$` символы, cligen считает их якорями |

libxml2 местами мягче стандарта XSD: например, `a{2,1}` (минимум больше максимума) он принимает.
Правило `xsd` находит паттерны, которые libxml2 не скомпилирует, а не любое отступление от стандарта.

```sh
$ yang-pattern-lint models/
models/example.yang:7:9: ошибка [mismatch]: XSD (libyang) и CLI clixon проверяют значения по-разному: '0083-11-31' по XSD подходит, CLI отклоняет (POSIX после перевода: '[0-9]{4}(0[1-9-]|1[0-2])(0[1-9-]|[12][0-9]|3[01])')
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

## Установка

Пакеты для Debian 12 и 13 публикуются на странице
[Releases](https://github.com/khromenokroman/yang-pattern-lint/releases). clixon и cligen в пакет
не входят и ставятся отдельно (см. «Зависимости»), libxml2 и libyang подтянет apt. Пакет
ставится только на свою версию Debian: libyang в Debian 12 и 13 разная (`libyang.so.2` и `libyang.so.3`).

```sh
wget https://github.com/khromenokroman/yang-pattern-lint/releases/download/v0.6.0.0/yang-pattern-lint_0.6.0.0_amd64-debian13.deb
sudo apt install ./yang-pattern-lint_0.6.0.0_amd64-debian13.deb
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
- **libxml2**: компиляция по XSD (`xmlRegexpCompile`), как у yanger.
- **libyang** >= 2.1 (`libyang2-dev` в Debian 12, `libyang-dev` в Debian 13): эталон смысла паттерна.

## Сборка

```sh
# Debian 12 и Astra: libyang2-dev, Debian 13: libyang-dev
LIBYANG_DEV=libyang2-dev
sudo apt install build-essential cmake flex bison pkg-config libssl-dev libfmt-dev libxml2-dev $LIBYANG_DEV libgtest-dev libgmock-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build
```

Пакет DEB: `cd build && cpack -G DEB`. Зависимости считает `dpkg-shlibdeps` по собранному
бинарнику, поэтому пакет годится только для той системы, где собран: в Debian 12 и Astra он
требует `libyang2` (`libyang.so.2`), в Debian 13 `libyang3` (`libyang.so.3`). Зависимость от
libclixon и libcligen в пакет не прописывается, они должны быть установлены отдельно.

## Использование

```sh
yang-pattern-lint [ключи] [ФАЙЛ.yang | КАТАЛОГ]...
```

| Ключ | Что делает |
|---|---|
| `--dump` | вывести найденные `pattern` с файлом и строкой, без проверки |
| `--disable ПРАВИЛА` | не выводить находки этих правил, через запятую |
| `--ignore-file ФАЙЛ` | файл исключений, по умолчанию `.yang-pattern-lint-ignore` |
| `-h`, `--help` | справка |
| `-V`, `--version` | версия |

Код возврата: 0 нет ошибок, 1 найдены ошибки, 2 неверные аргументы.

## Подавление находок

- Комментарий на строке `pattern` или на отдельной строке над ним:
  ```yang
  // yang-pattern-lint-disable mismatch
  pattern '[0-9]{4}\-[0-9]{2}';
  pattern '^[a-z]+$'; // yang-pattern-lint-disable
  ```
  Без перечня правил отключаются все. Метка в конце строки предыдущего оператора на следующий
  `pattern` не действует.
- Ключ `--disable anchors,escape` (можно повторять).
- Файл исключений: `--ignore-file ФАЙЛ`, по умолчанию `.yang-pattern-lint-ignore` в текущем каталоге.
  Строка: glob пути и правила через пробел или запятую, без правил подавляются все. Glob сравнивается
  с путём и с каждым его хвостом после `/`:
  ```
  # сторонние модели не трогаем
  vendor/*.yang
  models/legacy.yang anchors,mismatch
  ```

Предупреждения на код возврата не влияют, 1 возвращается только при ошибках.

## Релиз

Релиз собирает и публикует CI (`.github/workflows/release.yml`) по тегу `vВЕРСИЯ`.

1. Изменения слиты в `master`, CI на `master` зелёный.
2. Версия в `CMakeLists.txt` продвинута коммитом `build: Версия X.Y.Z.W`.
3. Поставить аннотированный тег на этот коммит и отправить его:

   ```sh
   git checkout master && git pull --ff-only
   git tag -a vX.Y.Z.W -m "yang-pattern-lint X.Y.Z.W"
   git push origin vX.Y.Z.W
   ```

4. CI по тегу проверяет, что тег совпадает с версией в `CMakeLists.txt`, собирает cligen и clixon,
   проект и тесты, собирает DEB для Debian 12 и 13 и публикует релиз с пакетами.

## Лицензия

GPL-3.0-or-later, см. [LICENSE](LICENSE).
