# Лабораторная работа №2, вариант 5

Кроссплатформенная лабораторная работа по курсу «Операционные системы».

Программа состоит из двух отдельных процессов:

- `parent` — родительский процесс;
- `child` — дочерний процесс.

Между процессами организован обмен данными через два однонаправленных канала:

- первый канал передаёт данные от `parent` к `child`;
- второй канал передаёт статус обработки от `child` к `parent`.

## Условие варианта

Родительский процесс:

1. Считывает имя выходного файла.
2. Затем принимает по одному целому числу.
3. Передаёт числа дочернему процессу через канал.

Дочерний процесс:

1. Получает число.
2. Проверяет его на простоту.
3. Если число составное — записывает его в файл.
4. Если число простое или отрицательное — отправляет сообщение родителю и завершает работу.

Числа `0` и `1` не записываются в файл, так как не являются ни простыми, ни составными.

## Особенности реализации

- Язык: C++17.
- Высокоуровневые обёртки над ОС не используются.
- `std::thread`, `std::filesystem` и аналогичные средства не применяются.
- Для Linux используются системные вызовы `pipe`, `fork`, `dup2`, `execl`, `read`, `write`, `waitpid`.
- Для Windows используются `CreatePipe`, `CreateProcessA`, `ReadFile`, `WriteFile`, `WaitForSingleObject`.
- Для безопасного освобождения ресурсов используются собственные RAII-классы для файловых дескрипторов и `HANDLE`.

## Структура проекта

```text
.
├── CMakeLists.txt
├── report.tex
├── styles/
│   └── log-style.sty
├── src/
│   ├── titlepage.tex
│   ├── problem.tex
│   ├── src.tex
│   ├── benchmark.tex
│   ├── thinks.tex
│   └── literature.tex
└── inc/
    ├── parent.cpp
    ├── child.cpp
    ├── common.cpp
    └── include/
        ├── common.hpp
        └── protocol.hpp
```

## Сборка

### Linux

```bash
cmake -S . -B build
cmake --build build
```

После сборки исполняемые файлы будут находиться в каталоге:

```text
build/bin/
```

### Windows

Можно использовать Visual Studio, CLion или сборку через CMake из командной строки:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Запуск

Из каталога проекта:

### Linux

```bash
./build/bin/parent
```

### Windows

```powershell
.\build\bin\Release\parent.exe
```

## Пример работы

```text
Output file: result.txt
Number: 4
Composite number was written to the file.
Number: 9
Composite number was written to the file.
Number: 1
0 and 1 are neither prime nor composite; value was ignored.
Number: 8
Composite number was written to the file.
Number: 7
Negative or prime number received. Processes are stopping.
```

После этого файл `result.txt` будет содержать:

```text
4
9
8
```

## Отчёт

Отчёт оформлен в LaTeX на основе шаблона `OS_report_template`.

Для компиляции отчёта:

```bash
pdflatex report.tex
```

или через Overleaf, загрузив проект целиком в виде ZIP-архива.