// ============================================================================
//   ПРАКТИКА №2. ПАТТЕРН «НАБЛЮДАТЕЛЬ» (OBSERVER) + СХЕМА MVC
//   Подробно закомментированная версия. Поведение совпадает с practice2.cpp.
// ============================================================================
//
// ЧТО ЭТА ПРОГРАММА ДЕЛАЕТ
// ------------------------
// Программа моделирует «распределение времени за день» — три категории:
// Учёба, Сон, Отдых, у каждой есть процент (в сумме ровно 100). Эти данные
// одновременно показываются в ТРЁХ видах:
//   1) таблица           → файл time_table.svg
//   2) гистограмма       → файл time_bars.svg
//   3) круговая диаграмма → файл time_pie.svg
//
// При запуске все три SVG-файла открываются в браузере. Пользователь через
// меню в консоли меняет значения (вручную или случайно), модель уведомляет
// все три вида, каждый вид перерисовывает свой файл. После этого в браузере
// нужно нажать F5, чтобы увидеть новые значения.
//
//
// ПАТТЕРН «НАБЛЮДАТЕЛЬ» (Observer) — В ДВУХ АБЗАЦАХ
// -------------------------------------------------
// Это поведенческий паттерн, в котором один объект (СУБЪЕКТ, у нас — Model)
// хранит список «подписчиков» (НАБЛЮДАТЕЛЕЙ, у нас — IObserver*). Когда
// субъект меняется, он перебирает всех наблюдателей и вызывает у каждого
// один и тот же метод (`update()`). Каждый наблюдатель сам решает, что
// делать в ответ.
//
// Это позволяет ОТВЯЗАТЬ модель от того, кто её отображает: модель не
// знает, что подписаны три SVG-вида (или пять, или ноль) — она просто
// уведомляет всех. Если завтра добавится 4-й вид (например, JSON-экспорт),
// модель НЕ меняется — нужно только создать новый класс-наблюдатель.
//
//
// СХЕМА MVC — В ОДНОМ АБЗАЦЕ
// --------------------------
// MVC (Model-View-Controller) — это разделение программы на три роли:
//   - Model: хранит данные и бизнес-логику (у нас — класс Model)
//   - View: отображает данные пользователю (у нас — 3 класса View)
//   - Controller: принимает действия пользователя и меняет модель
//                  (у нас — класс Controller)
// Связь: Controller → Model → (через Observer) → Views.
//
//
// КАРТА ФАЙЛА
// -----------
//  1. Заголовки                       — #include <…>
//  2. struct Slice                    — одна «доля»: имя + процент
//  3. class  IObserver                — абстрактный наблюдатель
//  4. class  Model                    — хранит данные + список наблюдателей
//  5. class  IView : public IObserver — базовый вид (умеет писать в SVG-файл)
//  6. class  TableView                — вид-таблица
//  7. class  BarChartView             — вид-гистограмма
//  8. class  PieChartView             — вид-круговая
//  9. class  Controller               — меню и работа с моделью
// 10. int    main()                   — собирает всё и запускает контроллер
//
// ============================================================================


// ---------------------------------------------------------------------------
// ЗАГОЛОВКИ
// ---------------------------------------------------------------------------
#include <algorithm>   // std::remove — для отписки наблюдателя
#include <cmath>       // std::cos, std::sin — для математики круговой диаграммы
#include <cstdlib>     // std::system — для запуска "start" (открыть SVG в браузере)
#include <fstream>     // std::ofstream — запись SVG в файл
#include <iostream>    // std::cout / std::cin — консольный ввод/вывод
#include <random>      // std::mt19937, std::uniform_int_distribution — случайные числа
#include <sstream>     // std::ostringstream — собирать большую строку из кусочков
#include <string>      // std::string
#include <utility>     // std::move, std::pair — перемещение и пара
#include <vector>      // std::vector — динамический массив


// ---------------------------------------------------------------------------
// СТРУКТУРА Slice — одна категория данных
// ---------------------------------------------------------------------------
// «Долька» данных: название и процент.
// struct — то же, что class, но все поля по умолчанию public.
struct Slice {
    std::string name;   // например, "Учёба"
    int percent;        // например, 30 (что значит 30%)
};


// ===========================================================================
// КЛАСС IObserver — АБСТРАКТНЫЙ НАБЛЮДАТЕЛЬ
// ===========================================================================
// Это минимальный «контракт» для всех, кто хочет получать уведомления от
// модели. В нём один метод update(), который модель вызовет, когда что-то
// изменится.
//
// Слово virtual делает метод полиморфным (правильная версия вызывается
// в зависимости от реального типа объекта). `= 0` означает «чисто
// виртуальный»: реализация будет в потомках.
//
// Виртуальный деструктор обязателен в любом классе, который планируется
// удалять через указатель на базу — иначе деструктор потомка не вызовется
// и будут утечки памяти.
class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void update() = 0;
};


// ===========================================================================
// КЛАСС Model — СУБЪЕКТ паттерна Observer + ХРАНИЛИЩЕ ДАННЫХ
// ===========================================================================
// Хранит вектор Slice (наши доли). Хранит вектор IObserver* (подписчики).
// Умеет:
//   - принимать/отписывать наблюдателей (attach / detach)
//   - выдавать текущие данные (slices)
//   - принимать новые проценты с валидацией (setPercents)
//   - оповещать всех (notify)
class Model {
    std::vector<Slice> slices_;          // сами данные
    std::vector<IObserver*> observers_;  // список подписчиков

public:
    // КОНСТРУКТОР. Принимает начальный вектор Slice (он будет «съеден»
    // через std::move — копирование не нужно).
    // explicit запрещает неявное преобразование (хорошая практика).
    explicit Model(std::vector<Slice> initial)
        : slices_(std::move(initial)) {}

    // ПОДПИСКА. Кладёт указатель на наблюдателя в список.
    void attach(IObserver* o) { observers_.push_back(o); }

    // ОТПИСКА. Идиома remove–erase: std::remove «отодвигает» все ненужные
    // элементы в конец и возвращает итератор на новый конец «нужной» части,
    // а vector::erase удаляет хвост. Это стандартный способ удалить
    // конкретное значение из vector в C++ до C++20.
    void detach(IObserver* o) {
        observers_.erase(std::remove(observers_.begin(), observers_.end(), o),
                         observers_.end());
    }

    // ГЕТТЕР. Возвращает ССЫЛКУ (& значит «по ссылке») на внутренний вектор.
    // `const` после метода: мы ничего не меняем. `const` у возвращаемого
    // вектора: вызывающий не сможет его испортить.
    const std::vector<Slice>& slices() const { return slices_; }

    // СМЕНА ДАННЫХ С ВАЛИДАЦИЕЙ.
    // Возвращает true, если данные приняты, false — если отвергнуты.
    bool setPercents(const std::vector<int>& percents) {
        // Проверка 1: правильное количество чисел
        if (percents.size() != slices_.size()) {
            std::cout << "Ошибка: ожидается " << slices_.size() << " чисел.\n";
            return false;
        }
        // Проверка 2: все числа неотрицательные + сумма = 100
        int sum = 0;
        for (int p : percents) {
            if (p < 0) {
                std::cout << "Ошибка: проценты должны быть >= 0.\n";
                return false;
            }
            sum += p;
        }
        if (sum != 100) {
            std::cout << "Ошибка: сумма должна равняться 100 (сейчас " << sum << ").\n";
            return false;
        }
        // Всё валидно — копируем значения и оповещаем подписчиков.
        for (std::size_t i = 0; i < slices_.size(); ++i) {
            slices_[i].percent = percents[i];
        }
        notify();
        return true;
    }

    // УВЕДОМЛЕНИЕ. Перебираем всех подписчиков и вызываем у каждого update().
    // Это и есть «сердце» паттерна Observer.
    void notify() {
        for (IObserver* o : observers_) {
            o->update();
        }
    }
};


// ===========================================================================
// БАЗОВЫЙ КЛАСС IView — «вид», ОН ЖЕ НАБЛЮДАТЕЛЬ
// ===========================================================================
// IView наследует от IObserver, значит каждый вид УМЕЕТ откликаться на
// уведомления (через update). По умолчанию update() пишет SVG-файл.
//
// Каждый конкретный вид должен сказать:
//   - filename()    — куда писать (имя файла)
//   - canvasSize()  — какой размер у SVG-«полотна»
//   - renderBody()  — что нарисовать внутри <svg>
//
// Сам writeFile() — общий и приватный. Это паттерн «шаблонный метод»:
// базовый класс задаёт скелет операции, потомки заполняют отдельные шаги.
class IView : public IObserver {
public:
    // update() переопределяет метод из IObserver — это override.
    // override — маркер для компилятора «проверь, что я действительно
    // что-то переопределяю». Если в базе функция изменится — будет ошибка.
    void update() override { writeFile(); }

    // Три «шага», которые обязан реализовать каждый конкретный вид:
    virtual std::string filename() const = 0;
    virtual std::pair<double, double> canvasSize() const = 0;
    virtual std::string renderBody() const = 0;

private:
    // Записать SVG в файл. const — метод не меняет состояние объекта.
    void writeFile() const {
        // structured binding (C++17): сразу распаковываем pair в две переменные.
        auto [w, h] = canvasSize();
        std::ofstream f(filename());  // открыли файл на запись
        f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        f << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w
          << "\" height=\"" << h << "\" viewBox=\"0 0 " << w << " " << h << "\">\n";
        // Белый фон во всё полотно — иначе будет прозрачно, и тёмная тема
        // браузера испортит цвета.
        f << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
        f << renderBody() << "\n";  // вот тут потомок рисует своё
        f << "</svg>\n";
        // f закроется автоматически в деструкторе ofstream.
    }
};


// ===========================================================================
// КЛАСС TableView — ВИД №1, ТАБЛИЦА
// ===========================================================================
// Двухстрочная таблица: верхняя строка — имена (Учёба/Сон/Отдых),
// нижняя — соответствующие проценты. Чёрная рамка вокруг, разделители
// между столбцами.
class TableView : public IView {
    const Model* model_;  // указатель на модель — откуда брать данные
    // Параметры размера ячейки и поля (static constexpr — вычисляются на
    // этапе компиляции, общие для всех экземпляров класса).
    static constexpr double CELL_W = 110.0;
    static constexpr double CELL_H = 50.0;
    static constexpr double PAD = 15.0;

public:
    explicit TableView(const Model* m) : model_(m) {}

    std::string filename() const override { return "time_table.svg"; }

    std::pair<double, double> canvasSize() const override {
        // Ширина = ширина одной ячейки * число долей + поля по краям.
        double w = CELL_W * model_->slices().size() + 2 * PAD;
        double h = 2 * CELL_H + 2 * PAD;
        return {w, h};
    }

    std::string renderBody() const override {
        const auto& slices = model_->slices();
        // ostringstream — это «строка, в которую можно писать как в поток».
        // Удобно собирать большой SVG из кусочков.
        std::ostringstream s;
        double x0 = PAD, y0 = PAD;
        double tableW = CELL_W * slices.size();
        double tableH = 2 * CELL_H;

        // Внешняя рамка таблицы.
        s << "<rect x=\"" << x0 << "\" y=\"" << y0
          << "\" width=\"" << tableW << "\" height=\"" << tableH
          << "\" fill=\"none\" stroke=\"black\" stroke-width=\"2\"/>\n";

        // Горизонтальная линия между шапкой и значениями.
        s << "<line x1=\"" << x0 << "\" y1=\"" << (y0 + CELL_H)
          << "\" x2=\"" << (x0 + tableW) << "\" y2=\"" << (y0 + CELL_H)
          << "\" stroke=\"black\" stroke-width=\"2\"/>\n";

        // Идём по столбцам: рисуем разделитель и две строки текста.
        for (std::size_t i = 0; i < slices.size(); ++i) {
            double cx = x0 + i * CELL_W;
            // Разделитель не нужен перед первым столбцом.
            if (i > 0) {
                s << "<line x1=\"" << cx << "\" y1=\"" << y0
                  << "\" x2=\"" << cx << "\" y2=\"" << (y0 + tableH)
                  << "\" stroke=\"black\" stroke-width=\"2\"/>\n";
            }
            // Имя категории по центру верхней ячейки.
            // text-anchor="middle" — выравнивание по центру относительно x.
            s << "<text x=\"" << (cx + CELL_W / 2) << "\" y=\"" << (y0 + CELL_H * 0.65)
              << "\" font-size=\"22\" text-anchor=\"middle\" font-family=\"serif\">"
              << slices[i].name << "</text>\n";
            // Значение процента в нижней ячейке.
            s << "<text x=\"" << (cx + CELL_W / 2) << "\" y=\"" << (y0 + CELL_H * 1.65)
              << "\" font-size=\"22\" text-anchor=\"middle\" font-family=\"serif\">"
              << slices[i].percent << "%</text>\n";
        }
        return s.str();  // возвращаем собранную строку
    }
};


// ===========================================================================
// КЛАСС BarChartView — ВИД №2, ГИСТОГРАММА
// ===========================================================================
// Вертикальная гистограмма с осями. На оси Y — проценты от 0 до 100
// с шагом 10, горизонтальные линии-сетка. Под каждой полосой — имя
// категории.
class BarChartView : public IView {
    const Model* model_;
    // Поля вокруг области графика
    static constexpr double MARGIN_L = 55.0;
    static constexpr double MARGIN_R = 25.0;
    static constexpr double MARGIN_T = 25.0;
    static constexpr double MARGIN_B = 55.0;
    static constexpr double BAR_W   = 65.0;   // ширина одной полосы
    static constexpr double BAR_GAP = 30.0;   // зазор между полосами
    static constexpr double MAX_PCT = 100.0;  // верх оси Y (в процентах)
    static constexpr double PLOT_H  = 260.0;  // высота области графика

public:
    explicit BarChartView(const Model* m) : model_(m) {}

    std::string filename() const override { return "time_bars.svg"; }

    std::pair<double, double> canvasSize() const override {
        std::size_t n = model_->slices().size();
        // Ширина области графика = N полос + (N+1) зазоров.
        double plotW = n * BAR_W + (n + 1) * BAR_GAP;
        return {MARGIN_L + plotW + MARGIN_R, MARGIN_T + PLOT_H + MARGIN_B};
    }

    std::string renderBody() const override {
        const auto& slices = model_->slices();
        std::ostringstream s;
        std::size_t n = slices.size();
        double plotW = n * BAR_W + (n + 1) * BAR_GAP;
        double y0 = MARGIN_T + PLOT_H;  // нижний край области (y «0%»)

        // СЕТКА И МЕТКИ ОСИ Y. Идём от 0% до 100% с шагом 10.
        for (int p = 0; p <= 100; p += 10) {
            // SVG-координаты: y растёт ВНИЗ, поэтому 0% внизу (y0), 100% вверху.
            double y = y0 - (p / MAX_PCT) * PLOT_H;
            // Тонкая серая линия-сетка через всю ширину.
            s << "<line x1=\"" << MARGIN_L << "\" y1=\"" << y
              << "\" x2=\"" << (MARGIN_L + plotW) << "\" y2=\"" << y
              << "\" stroke=\"lightgray\" stroke-width=\"1\"/>\n";
            // Подпись «50%» и т.п. слева от оси.
            s << "<text x=\"" << (MARGIN_L - 8) << "\" y=\"" << (y + 5)
              << "\" font-size=\"14\" text-anchor=\"end\" font-family=\"sans-serif\">"
              << p << "%</text>\n";
        }

        // ОСИ X и Y (жирные чёрные линии).
        s << "<line x1=\"" << MARGIN_L << "\" y1=\"" << MARGIN_T
          << "\" x2=\"" << MARGIN_L << "\" y2=\"" << y0
          << "\" stroke=\"black\" stroke-width=\"2\"/>\n";
        s << "<line x1=\"" << MARGIN_L << "\" y1=\"" << y0
          << "\" x2=\"" << (MARGIN_L + plotW) << "\" y2=\"" << y0
          << "\" stroke=\"black\" stroke-width=\"2\"/>\n";

        // ПОЛОСЫ. Для каждой категории — синий прямоугольник нужной высоты.
        for (std::size_t i = 0; i < n; ++i) {
            double bx = MARGIN_L + BAR_GAP + i * (BAR_W + BAR_GAP);
            double bh = (slices[i].percent / MAX_PCT) * PLOT_H;
            double by = y0 - bh;  // верхний край полосы (полоса растёт ВВЕРХ от оси X)
            s << "<rect x=\"" << bx << "\" y=\"" << by
              << "\" width=\"" << BAR_W << "\" height=\"" << bh
              << "\" fill=\"#4a76c5\"/>\n";  // фирменный «оффисный» синий
            // Имя категории под полосой.
            s << "<text x=\"" << (bx + BAR_W / 2) << "\" y=\"" << (y0 + 28)
              << "\" font-size=\"18\" text-anchor=\"middle\" font-family=\"serif\">"
              << slices[i].name << "</text>\n";
        }
        return s.str();
    }
};


// ===========================================================================
// КЛАСС PieChartView — ВИД №3, КРУГОВАЯ ДИАГРАММА
// ===========================================================================
// Круг радиусом R с центром (CX, CY). Для каждого процента считаем сектор:
//   - сектор начинается с угла startAng (в градусах),
//   - длина сектора = процент * 3.6 (1% = 3.6° потому что 100% = 360°),
//   - находим x1, y1 (начало дуги) и x2, y2 (конец дуги) через cos/sin,
//   - рисуем SVG-путь: «M центр → L первая_точка → A (дуга) → Z (замкнуть)».
class PieChartView : public IView {
    const Model* model_;
    static constexpr double CX = 200.0;
    static constexpr double CY = 200.0;
    static constexpr double R  = 140.0;
    // π с большой точностью. C++ до 20 стандарта не даёт M_PI как часть
    // стандарта (хотя многие компиляторы предоставляют), поэтому пишем сами.
    static constexpr double PI = 3.14159265358979323846;

public:
    explicit PieChartView(const Model* m) : model_(m) {}

    std::string filename() const override { return "time_pie.svg"; }

    std::pair<double, double> canvasSize() const override {
        // Фиксированный размер «полотна»: квадрат под круг + место под легенду.
        return {400.0, 440.0};
    }

    std::string renderBody() const override {
        const auto& slices = model_->slices();
        // Палитра цветов. Если категорий больше 5, цикл повторится.
        const std::string colors[] = {"#4a76c5", "#ed7d31", "#a5a5a5", "#ffc000", "#70ad47"};
        std::ostringstream s;

        // Начинаем с угла -90° (12 часов на циферблате), идём по часовой стрелке.
        double startAng = -90.0;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            // sweep = угол, который займёт этот сектор (в градусах).
            double sweep = slices[i].percent * 3.6;
            double endAng = startAng + sweep;

            // Считаем декартовы координаты начала и конца дуги.
            // Тригонометрия принимает РАДИАНЫ, поэтому * PI / 180.
            double x1 = CX + R * std::cos(startAng * PI / 180.0);
            double y1 = CY + R * std::sin(startAng * PI / 180.0);
            double x2 = CX + R * std::cos(endAng   * PI / 180.0);
            double y2 = CY + R * std::sin(endAng   * PI / 180.0);

            // SVG-путь Arc имеет флаг largeArc: 0 = меньшая дуга, 1 = большая.
            // Если сектор больше полукруга — нужна большая дуга.
            int largeArc = (sweep > 180.0) ? 1 : 0;

            // Особый случай: ровно 100% — рисуем полный круг (иначе path
            // получает совпадающие точки и сектор не отрисуется).
            if (slices[i].percent == 100) {
                s << "<circle cx=\"" << CX << "\" cy=\"" << CY
                  << "\" r=\"" << R << "\" fill=\"" << colors[i % 5]
                  << "\" stroke=\"white\" stroke-width=\"2\"/>\n";
            } else if (slices[i].percent > 0) {
                // Команды пути:
                //   M cx cy        — перо в центр
                //   L x1 y1        — линия до начала дуги
                //   A r r 0 lA 1 x2 y2 — дуга радиуса r, угол поворота 0,
                //                        флаг большой дуги, направление 1 (по часовой),
                //                        до точки (x2, y2)
                //   Z              — замкнуть путь обратно к началу (в центр)
                s << "<path d=\"M " << CX << " " << CY
                  << " L " << x1 << " " << y1
                  << " A " << R << " " << R << " 0 " << largeArc << " 1 "
                  << x2 << " " << y2 << " Z\" "
                  << "fill=\"" << colors[i % 5] << "\" stroke=\"white\" stroke-width=\"2\"/>\n";
            }
            // Готовимся к следующему сектору — он начнётся там, где кончился текущий.
            startAng = endAng;
        }

        // ЛЕГЕНДА: маленькие цветные квадратики с подписями внизу.
        double lx = 30, ly = 380;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            double sx = lx + i * 120;
            s << "<rect x=\"" << sx << "\" y=\"" << ly
              << "\" width=\"14\" height=\"14\" fill=\"" << colors[i % 5] << "\"/>\n";
            s << "<text x=\"" << (sx + 20) << "\" y=\"" << (ly + 12)
              << "\" font-size=\"15\" font-family=\"serif\">"
              << slices[i].name << "</text>\n";
        }
        return s.str();
    }
};


// ===========================================================================
// КЛАСС Controller — РЕАГИРУЕТ НА КОМАНДЫ ПОЛЬЗОВАТЕЛЯ
// ===========================================================================
// Хранит указатель на модель. В run() крутится цикл меню, принимающий
// команды:
//   1) ввести проценты вручную   → model->setPercents(ввод)
//   2) случайные                 → сгенерировать и вызвать setPercents
//   0) выход                     → break из цикла
class Controller {
    Model* model_;
    // Генератор случайных чисел. mt19937 — известный «вихрь Мерсенна»,
    // быстрый и качественный. Стартуем от случайного «зерна», полученного
    // из std::random_device (читает энтропию ОС).
    std::mt19937 rng_;

public:
    explicit Controller(Model* m)
        : model_(m), rng_(std::random_device{}()) {}
        // ^ {} — это default-constructed random_device, потом ()  его вызывает,
        //   возвращая случайное число, которым инициализируется rng_.

    void run() {
        while (true) {
            std::cout << "\n=== Распределение времени ===\n";
            std::cout << "1) ввести проценты вручную\n";
            std::cout << "2) случайные значения\n";
            std::cout << "0) выход\n";
            std::cout << "> ";

            int cmd;
            // Если cin не смог прочитать число (пользователь ввёл буквы) —
            // его состояние «портится». Чистим: clear() сбрасывает флаги,
            // getline съедает мусорную строку.
            if (!(std::cin >> cmd)) {
                std::cin.clear();
                std::string trash;
                std::getline(std::cin, trash);
                std::cout << "Введите число.\n";
                continue;
            }

            if (cmd == 0) break;
            else if (cmd == 1) readManual();
            else if (cmd == 2) randomValues();
            else std::cout << "Неизвестная команда.\n";
        }
    }

private:
    void readManual() {
        std::size_t n = model_->slices().size();
        std::cout << "Введите " << n << " чисел (сумма = 100): ";
        std::vector<int> ps(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (!(std::cin >> ps[i])) {
                std::cin.clear();
                std::string trash;
                std::getline(std::cin, trash);
                std::cout << "Ошибка ввода.\n";
                return;
            }
        }
        // Модель сама проверит сумму и неотрицательность. Если ок —
        // вызовет notify() и все 3 вида перерисуются.
        if (model_->setPercents(ps)) {
            std::cout << "Обновлено. Нажмите F5 в браузере.\n";
        }
    }

    void randomValues() {
        // Генерируем n чисел, сумма которых = 100.
        // Простой способ: первое — случайно от 0 до 100, второе — от 0
        // до (100 - первое), и так далее. Последнее = то, что осталось.
        std::size_t n = model_->slices().size();
        std::vector<int> ps(n);
        int remaining = 100;
        for (std::size_t i = 0; i + 1 < n; ++i) {
            std::uniform_int_distribution<int> dist(0, remaining);
            ps[i] = dist(rng_);
            remaining -= ps[i];
        }
        ps[n - 1] = remaining;

        std::cout << "Сгенерировано: ";
        for (int p : ps) std::cout << p << " ";
        std::cout << "\n";

        model_->setPercents(ps);
        std::cout << "Нажмите F5 в браузере.\n";
    }
};


// ===========================================================================
// ФУНКЦИЯ main — собирает всё MVC и запускает программу
// ===========================================================================
int main() {
    // chcp 65001 — команда Windows: «переключи консоль в UTF-8»,
    // иначе русские буквы отобразятся кракозябрами. > nul — глушим
    // сообщение «Active code page: 65001».
    std::system("chcp 65001 > nul");

    // СОЗДАЁМ МОДЕЛЬ с начальными данными (сумма = 100).
    Model model({
        {"Учёба", 30},
        {"Сон",   35},
        {"Отдых", 35}
    });

    // СОЗДАЁМ ТРИ ВИДА. Каждый получает указатель на модель — он будет
    // читать оттуда данные при отрисовке.
    TableView    table(&model);
    BarChartView bars(&model);
    PieChartView pie(&model);

    // ПОДПИСЫВАЕМ ВИДЫ НА МОДЕЛЬ. Теперь model.notify() позовёт у каждого update().
    model.attach(&table);
    model.attach(&bars);
    model.attach(&pie);

    // ПЕРВИЧНАЯ ОТРИСОВКА. Уведомляем — все три вида запишут свои SVG-файлы.
    model.notify();

    // ОТКРЫВАЕМ КАЖДЫЙ SVG В БРАУЗЕРЕ. system("start ...") — команда cmd,
    // открывает файл программой по умолчанию (для .svg это обычно браузер).
    // Пустые "" — это «заголовок окна» (нужен для синтаксиса start).
    std::system("start \"\" time_table.svg");
    std::system("start \"\" time_bars.svg");
    std::system("start \"\" time_pie.svg");

    // ЗАПУСКАЕМ КОНТРОЛЛЕР. В нём цикл меню до выхода.
    Controller c(&model);
    c.run();

    // Все объекты (model, table, bars, pie, c) — локальные в main(),
    // уничтожатся автоматически при выходе из функции.
    return 0;
}
