# Information-systems-and-technologies

Практики по курсу «Информационные системы и технологии», 6 семестр, ОмГУ.
Все задания на C++17.

## Подготовка (один раз за сессию PowerShell)

Компилятор — `g++` из MSYS2 UCRT64. Добавляем его в `PATH` для текущего окна:

```powershell
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
```

После этого `g++` будет вызываться по короткому имени. Все команды ниже запускать из корня репозитория.


```
$env:Path = "C:\msys64\ucrt64\bin;$env:Path"
g++ --version
Get-MpComputerStatus | Select-Object SmartAppControlState
```

## Практика 1 — паттерн «Компоновщик» (Composite)
Математические формулы (дробь, интеграл, матрица) как дерево; рендерится в SVG.

```powershell
cd practice1
g++ -std=c++17 -O2 main.cpp -o formula.exe
.\formula.exe
cd ..
```
Результат: `result.svg` (откроется автоматически).

## Практика 2 — паттерн «Наблюдатель» (Observer) + MVC
Таблица времени работы как Subject; три View (таблица, столбцы, круг) подписаны и сами перерисовываются.

```powershell
cd practice2
g++ -std=c++17 -O2 practice2.cpp -o practice2.exe
.\practice2.exe
cd ..
```
Результат: `time_table.svg`, `time_bars.svg`, `time_pie.svg`.

## Практика 3 — паттерн «Декоратор» (Decorator)
Те же три View, но с навешиваемыми декораторами (рамка, заголовок, подсветка).

```powershell
cd practice3
g++ -std=c++17 -O2 practice3.cpp -o practice3.exe
.\practice3.exe
cd ..
```
Результат: `time_table.svg`, `time_bars.svg`, `time_pie.svg` (уже с декорациями).

## Практика 4 — паттерн «Цепочка обязанностей» (Chain of Responsibility)
К фигурам из Практики 1 добавлена обработка запросов `Print`/`Help` через `parent_`-цепочку. Рассылка стартует с листьев и идёт вверх по дереву.

```powershell
cd practice4_chain
g++ -std=c++17 -O2 practice4_chain.cpp -o practice4_chain.exe
.\practice4_chain.exe
cd ..
```
Программа открывает `chain.svg` в браузере и печатает bbox каждой фигуры. Команды в консоли:
- `h X Y` — Help (описание фигуры в консоль)
- `p X Y` — Print (запись описания в `print.log`)
- `q` — выход

## Если что-то не работает

- `g++ : The term 'g++' is not recognized` — не выполнен блок «Подготовка». Запусти его в текущем окне PowerShell.
- `formula.exe : The term 'formula.exe' is not recognized` — в PowerShell бинарь из текущей папки нужно вызывать с префиксом: `.\formula.exe`.
- `cannot open output file ... Permission denied` — старый `.exe` уже запущен. Закрой окно с консолью программы и пересобери.

## Структура папок

В каждой папке `practiceN/`:
- `*.cpp` — основной код
- `*_commented.cpp` — то же самое с подробными комментариями для разбора
- `*.exe` — собранный бинарь
- `defense_qa_*.md` — шпаргалка с вопросами-ответами для защиты
- `*.svg` — выход программы
