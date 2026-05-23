// ============================================================================
//   ПРАКТИКА №4 (вариант 1). ПАТТЕРН «ЦЕПОЧКА ОБЯЗАННОСТЕЙ»
//   (Chain of Responsibility)
//   Подробно закомментированная версия. Поведение совпадает с practice4_chain.cpp.
// ============================================================================
//
// ЧТО ЭТА ПРОГРАММА ДЕЛАЕТ
// ------------------------
// Берём то же дерево фигур, что в Практике 1. К каждой фигуре добавляем
// возможность принимать ДВА типа запросов:
//   • HandlePrint(x, y) — записать описание фигуры в файл "print.log"
//   • HandleHelp(x, y)  — вывести описание в консоль
//
// Пользователь вводит координату (x, y) в пикселях SVG. Запрос обрабатывает
// та формула, в чей прямоугольник (bbox) попадает точка И которая лежит
// «глубже всех» (= визуально выше). Если точка вне всех формул — запрос
// теряется.
//
//
// КАК РЕАЛИЗОВАНА ЦЕПОЧКА (по канону GoF и подсказке Сергея)
// ----------------------------------------------------------
// 1) В базовом классе Formula есть поле  Formula* parent_  — указатель на
//    родителя в дереве-компоновщике. Корень дерева имеет parent_ == nullptr.
//    Каждый композит (Fraction, Integral, Matrix) в своём конструкторе зовёт
//    child->setParent(this), чтобы дерево было двусвязным.
//
// 2) Метод  Formula::bubbleUp(t, x, y)  — это и есть КЛАССИЧЕСКАЯ Chain of
//    Responsibility: узел проверяет, содержит ли он точку. Если да —
//    обрабатывает. Если нет — вызывает parent_->bubbleUp(...). Если parent_
//    == nullptr и не попал — возвращает false (запрос «вышел из цепочки»).
//
// 3) Диспетчер  dispatch()  работает в две фазы. Сначала собирает все ЛИСТЬЯ
//    дерева (Atom-ы) и проверяет, не содержит ли какой-то лист точку
//    напрямую (= визуально верхняя фигура). Если да — этот лист обрабатывает.
//    Если ни один лист не попал — поднимаемся вверх по parent_ от каждого
//    листа и выбираем самого ГЛУБОКОГО предка, в чей bbox попала точка.
//    Этот предок и обрабатывает.
//
//    Зачем две фазы. Если бы мы просто пробежали bubbleUp от каждого листа,
//    был бы баг: лист соседнего поддерева (где точки нет) поднялся бы до
//    общего предка и захватил запрос раньше, чем мы вообще доберёмся до
//    нужного контейнера. Так бы запрос попал к корню, а не к Matrix.
//
//
// СВЯЗЬ С ПРАКТИКОЙ 1
// -------------------
// Существующие классы (Atom, Fraction, Integral, Matrix) почти не изменились,
// добавилось:
//   • mutable-поля x_, y_, w_, h_ в Formula — для запоминания позиции
//   • поле Formula* parent_ + setParent()/parent()
//   • виртуальный typeName() (имя для отчётов)
//   • виртуальный children() (для обхода дерева)
//   • вызовы setParent(this) в конструкторах композитов и в Matrix::setCell
// Логика отрисовки идентична Практике 1, поэтому SVG выглядит так же.
//
//
// КАРТА ФАЙЛА
// -----------
//   1. Заголовки
//   2. struct Box, enum RequestType
//   3. class Formula           — базовый класс + ядро Chain of Responsibility
//   4. class Atom, Fraction, Integral, Matrix — содержательная часть
//   5. collectLeaves, depthOf, dispatch — диспетчер запросов
//   6. printBoxes, writeSvg    — отладка и рендеринг
//   7. int main()              — собирает дерево + цикл запросов
// ============================================================================


#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>


// Размер фигуры при отрисовке. То же, что в Практике 1.
struct Box {
    double w;
    double h;
    double baseline;
};


// Тип запроса. enum class — «строгий» enum: имя пишется через RequestType::Print,
// и его нельзя случайно сравнить с int.
enum class RequestType { Print, Help };


// ===========================================================================
// БАЗОВЫЙ КЛАСС Formula — узел дерева + ЯДРО ЦЕПОЧКИ ОБЯЗАННОСТЕЙ
// ===========================================================================
class Formula {
protected:
    // mutable — поля, которые можно менять даже у const-объекта. Нужно потому,
    // что draw() помечен const (логически он «только рисует»), но при этом
    // ему нужно ЗАПИСАТЬ собственную позицию и размер, чтобы потом, при
    // обработке запроса, можно было проверить «попала ли точка в меня».
    mutable double x_ = 0;
    mutable double y_ = 0;
    mutable double w_ = 0;
    mutable double h_ = 0;

    // Указатель на родителя в дереве-компоновщике. У корня == nullptr.
    // Это ключевое поле для канонической Chain of Responsibility:
    // запрос идёт по цепочке parent_ → parent_->parent_ → ... → корень.
    Formula* parent_ = nullptr;

public:
    virtual ~Formula() = default;

    // Чисто виртуальные методы из Практики 1: измерить и нарисовать.
    virtual Box measure() const = 0;
    virtual void draw(std::ostream& out, double x, double y) const = 0;

    // Имя типа («Дробь», «Интеграл», ...) — для сообщений в Help/Print.
    virtual std::string typeName() const = 0;

    // Список дочерних узлов. Atom-ы возвращают пустой список (по умолчанию).
    // Композиты (Fraction, Integral, Matrix) переопределяют.
    virtual std::vector<const Formula*> children() const { return {}; }

    // Установить родителя. Зовётся композитом в конструкторе для каждого
    // ребёнка. После этого дерево становится двусвязным: ребёнок знает
    // родителя через parent_, родитель — детей через children().
    void setParent(Formula* p) { parent_ = p; }
    Formula* parent() const { return parent_; }

    // ПРОВЕРКА «ТОЧКА ВНУТРИ МЕНЯ?»
    // Использует прямоугольник, записанный при последнем draw().
    bool contains(int px, int py) const {
        double x = static_cast<double>(px);
        double y = static_cast<double>(py);
        return x >= x_ && x < x_ + w_ && y >= y_ && y < y_ + h_;
    }

    // Описание для HandleHelp. Строка вида:
    // "Я Дробь, позиция (30,30), размер 156x111".
    std::string describe() const {
        return "Я " + typeName() +
               ", позиция (" + std::to_string(static_cast<int>(x_)) +
               "," + std::to_string(static_cast<int>(y_)) +
               "), размер " + std::to_string(static_cast<int>(w_)) +
               "x" + std::to_string(static_cast<int>(h_));
    }

    static const char* requestName(RequestType t) {
        return t == RequestType::Print ? "Print" : "Help";
    }

    // -----------------------------------------------------------------
    // СЕРДЦЕ ПАТТЕРНА: bubbleUp — пройти ВВЕРХ по цепочке parent_.
    // -----------------------------------------------------------------
    // Каждый узел делает одно из двух:
    //   • contains(x, y) == true → обрабатывает сам, возвращает true.
    //   • иначе → перекидывает запрос родителю: parent_->bubbleUp(...).
    // Если parent_ == nullptr (мы на корне и тот не попал) → false.
    //
    // Это и есть «запросы посылаются от потомков к родителям» из методички.
    bool bubbleUp(RequestType t, int x, int y) const {
        std::cout << "    -> " << typeName();
        if (contains(x, y)) {
            std::cout << " ПОПАЛ — обрабатываю\n";
            doHandle(t, x, y);
            return true;
        }
        std::cout << " мимо, передаю выше\n";
        if (parent_) return parent_->bubbleUp(t, x, y);
        return false;
    }

    // То же, что bubbleUp, но БЕЗ обработки и без печати. Нужно диспетчеру
    // (см. dispatch ниже), чтобы выбрать самого глубокого подходящего предка
    // из нескольких цепочек, не обрабатывая запрос несколько раз.
    const Formula* findHandler(int x, int y) const {
        if (contains(x, y)) return this;
        if (parent_) return parent_->findHandler(x, y);
        return nullptr;
    }

private:
    // ФУНКЦИЯ ДИСПЕТЧЕРИЗАЦИИ (примечание 2 методички): switch по типу запроса.
    //   Help  → описать себя в консоль
    //   Print → дописать описание в файл print.log
    void doHandle(RequestType t, int x, int y) const {
        if (t == RequestType::Help) {
            std::cout << "  [Help " << x << "," << y << "] " << describe() << "\n";
        } else {
            std::ofstream f("print.log", std::ios::app);  // append-режим
            f << "[Print " << x << "," << y << "] " << describe() << "\n";
            std::cout << "  [Print " << x << "," << y
                      << "] записано в print.log: " << describe() << "\n";
        }
    }
};

using FormulaPtr = std::unique_ptr<Formula>;


// ===========================================================================
//  ATOM — лист (буква/число)
// ===========================================================================
// Дополнения для CoR:
//   • typeName() возвращает "Atom('x')"
//   • draw() записывает свою позицию в x_/y_/w_/h_
//   • children() наследуется из базы — возвращает пустой список (атом — лист)
//   • parent_ устанавливает родитель при добавлении в композит
class Atom : public Formula {
    std::string text_;
    double fontSize_;

public:
    explicit Atom(std::string text, double fontSize = 22.0)
        : text_(std::move(text)), fontSize_(fontSize) {}

    std::string typeName() const override {
        return "Atom('" + text_ + "')";
    }

    Box measure() const override {
        double w = static_cast<double>(text_.size()) * fontSize_ * 0.55;
        double h = fontSize_ * 1.2;
        double baseline = fontSize_ * 0.95;
        return {w, h, baseline};
    }

    void draw(std::ostream& out, double x, double y) const override {
        Box b = measure();
        // ВАЖНО: запоминаем абсолютные координаты — для contains().
        x_ = x; y_ = y; w_ = b.w; h_ = b.h;
        out << "<text x=\"" << x << "\" y=\"" << (y + b.baseline)
            << "\" font-family=\"Cambria Math, Times, serif\" font-style=\"italic\""
            << " font-size=\"" << fontSize_ << "\">" << text_ << "</text>\n";
    }
};


// ===========================================================================
//  FRACTION — дробь (Composite)
// ===========================================================================
class Fraction : public Formula {
    FormulaPtr top_;
    FormulaPtr bottom_;
    static constexpr double GAP = 4.0;
    static constexpr double SIDE = 8.0;

public:
    // В конструкторе ВЫСТРАИВАЕМ ДВУСВЯЗНОЕ ДЕРЕВО:
    // дробь становится родителем для числителя и знаменателя.
    Fraction(FormulaPtr top, FormulaPtr bottom)
        : top_(std::move(top)), bottom_(std::move(bottom)) {
        top_->setParent(this);
        bottom_->setParent(this);
    }

    std::string typeName() const override { return "Дробь"; }

    // .get() — получить сырой указатель из unique_ptr, не отбирая владение.
    std::vector<const Formula*> children() const override {
        return {top_.get(), bottom_.get()};
    }

    Box measure() const override {
        Box t = top_->measure();
        Box b = bottom_->measure();
        double w = std::max(t.w, b.w) + 2 * SIDE;
        double h = t.h + 2 * GAP + b.h;
        return {w, h, t.h + GAP};
    }

    void draw(std::ostream& out, double x, double y) const override {
        Box me = measure();
        x_ = x; y_ = y; w_ = me.w; h_ = me.h;  // запоминаем для contains
        Box t = top_->measure();
        Box b = bottom_->measure();
        top_->draw(out, x + (me.w - t.w) / 2.0, y);
        double barY = y + t.h + GAP;
        out << "<line x1=\"" << x << "\" y1=\"" << barY
            << "\" x2=\"" << (x + me.w) << "\" y2=\"" << barY
            << "\" stroke=\"black\" stroke-width=\"1.5\"/>\n";
        bottom_->draw(out, x + (me.w - b.w) / 2.0, barY + GAP);
    }
};


// ===========================================================================
//  INTEGRAL — интеграл (Composite)
// ===========================================================================
class Integral : public Formula {
    FormulaPtr body_;
    FormulaPtr diff_;
    static constexpr double V_PAD = 6.0;
    static constexpr double GAP = 6.0;

public:
    Integral(FormulaPtr body, FormulaPtr diff)
        : body_(std::move(body)), diff_(std::move(diff)) {
        body_->setParent(this);
        diff_->setParent(this);
    }

    std::string typeName() const override { return "Интеграл"; }

    std::vector<const Formula*> children() const override {
        return {body_.get(), diff_.get()};
    }

    Box measure() const override {
        Box b = body_->measure();
        Box d = diff_->measure();
        double h = std::max(b.h, d.h) + 2 * V_PAD;
        double signW = h * 0.4;
        double w = signW + GAP + b.w + GAP + d.w;
        return {w, h, h / 2.0};
    }

    void draw(std::ostream& out, double x, double y) const override {
        Box me = measure();
        x_ = x; y_ = y; w_ = me.w; h_ = me.h;
        Box b = body_->measure();
        Box d = diff_->measure();
        double signW = me.h * 0.4;
        double signFont = me.h * 0.95;
        out << "<text x=\"" << x << "\" y=\"" << (y + me.h * 0.85)
            << "\" font-family=\"Cambria Math, Times, serif\""
            << " font-size=\"" << signFont
            << "\" font-weight=\"100\">&#8747;</text>\n";
        double bodyX = x + signW + GAP;
        double bodyY = y + (me.h - b.h) / 2.0;
        body_->draw(out, bodyX, bodyY);
        double diffX = bodyX + b.w + GAP;
        double diffY = y + (me.h - d.h) / 2.0;
        diff_->draw(out, diffX, diffY);
    }
};


// ===========================================================================
//  MATRIX — матрица 3×3 (Composite)
// ===========================================================================
class Matrix : public Formula {
    std::array<std::array<FormulaPtr, 3>, 3> cells_;
    static constexpr double CELL_H_GAP = 16.0;
    static constexpr double CELL_V_GAP = 10.0;
    static constexpr double BRACKET_W = 12.0;
    static constexpr double INNER_PAD = 6.0;

    void grid(std::array<double, 3>& colW, std::array<double, 3>& rowH) const {
        colW.fill(0);
        rowH.fill(0);
        for (int r = 0; r < 3; ++r) {
            for (int c = 0; c < 3; ++c) {
                Box b = cells_[r][c]->measure();
                colW[c] = std::max(colW[c], b.w);
                rowH[r] = std::max(rowH[r], b.h);
            }
        }
    }

public:
    // setCell вызывается ПОСЛЕ создания матрицы. Тоже устанавливает parent_,
    // чтобы ячейки знали, кто их родитель в дереве.
    void setCell(int r, int c, FormulaPtr child) {
        child->setParent(this);
        cells_[r][c] = std::move(child);
    }

    std::string typeName() const override { return "Матрица 3x3"; }

    // У матрицы 9 детей — все ячейки по очереди.
    std::vector<const Formula*> children() const override {
        std::vector<const Formula*> v;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                v.push_back(cells_[r][c].get());
        return v;
    }

    Box measure() const override {
        std::array<double, 3> colW{}, rowH{};
        grid(colW, rowH);
        double w = 2 * BRACKET_W + 2 * INNER_PAD;
        for (int c = 0; c < 3; ++c) {
            w += colW[c];
            if (c < 2) w += CELL_H_GAP;
        }
        double h = 2 * INNER_PAD;
        for (int r = 0; r < 3; ++r) {
            h += rowH[r];
            if (r < 2) h += CELL_V_GAP;
        }
        return {w, h, h / 2.0};
    }

    void draw(std::ostream& out, double x, double y) const override {
        std::array<double, 3> colW{}, rowH{};
        grid(colW, rowH);
        Box me = measure();
        x_ = x; y_ = y; w_ = me.w; h_ = me.h;

        // Левая скобка [
        out << "<path d=\"M " << (x + BRACKET_W) << " " << y
            << " Q " << x << " " << (y + me.h / 2.0)
            << " " << (x + BRACKET_W) << " " << (y + me.h)
            << "\" fill=\"none\" stroke=\"black\" stroke-width=\"1.5\"/>\n";

        // Правая скобка ]
        double rx = x + me.w;
        out << "<path d=\"M " << (rx - BRACKET_W) << " " << y
            << " Q " << rx << " " << (y + me.h / 2.0)
            << " " << (rx - BRACKET_W) << " " << (y + me.h)
            << "\" fill=\"none\" stroke=\"black\" stroke-width=\"1.5\"/>\n";

        // Раскладка ячеек по сетке.
        double curY = y + INNER_PAD;
        for (int r = 0; r < 3; ++r) {
            double curX = x + BRACKET_W + INNER_PAD;
            for (int c = 0; c < 3; ++c) {
                Box cb = cells_[r][c]->measure();
                double cx = curX + (colW[c] - cb.w) / 2.0;
                double cy = curY + (rowH[r] - cb.h) / 2.0;
                cells_[r][c]->draw(out, cx, cy);
                curX += colW[c] + CELL_H_GAP;
            }
            curY += rowH[r] + CELL_V_GAP;
        }
    }
};


// ---------------------------------------------------------------------------
//  collectLeaves — рекурсивно собрать все листья дерева (Atom-ы).
// ---------------------------------------------------------------------------
// Лист = узел без детей. Используется диспетчером для рассылки запроса.
void collectLeaves(const Formula* node, std::vector<const Formula*>& out) {
    auto kids = node->children();
    if (kids.empty()) {
        out.push_back(node);
        return;
    }
    for (auto k : kids) collectLeaves(k, out);
}

// Глубина узла (длина цепочки parent_ от него до корня + 1).
// Используется, чтобы выбрать самого «глубокого» предка из нескольких цепочек.
int depthOf(const Formula* n) {
    int d = 0;
    while (n) { ++d; n = n->parent(); }
    return d;
}


// ===========================================================================
//  ДИСПЕТЧЕР ЗАПРОСОВ — главная точка входа
// ===========================================================================
//
// Сергей сказал: «в каждый узел добавить ссылку на родителя, чтобы можно было
// ходить по дереву снизу вверх. А рассылку начинать с листьев.»
//
// Поэтому:
//   1) Собираем все листья (Atom-ы).
//   2) ФАЗА 1. Какой лист сам содержит точку? Если есть — он визуально
//      «верхняя» фигура (мельче всех вложенных), и он обрабатывает.
//      Вызываем leaf->bubbleUp(t, x, y) — на первом же узле (самом листе)
//      bubbleUp срабатывает с contains==true и обрабатывает.
//   3) ФАЗА 2. Ни один лист не попал — значит точка лежит ВНУТРИ какого-то
//      контейнера, но не внутри его листьев (например, на скобке матрицы
//      или между ячейками). Тогда поднимаемся вверх ОТ КАЖДОГО листа по
//      parent_ и ищем САМОГО ГЛУБОКОГО предка, в чей bbox попала точка.
//      Это и есть «топовый» контейнер на экране. Его и просим обработать.
//
// Почему две фазы. Если бы делали наивно «for each leaf: leaf->bubbleUp»,
// получили бы баг: листья соседних поддеревьев (например, x и dx в Integral
// при клике в Matrix) поднимались бы по цепочке через свой Integral до
// общего корня Fraction и захватывали бы запрос — а правильнее, чтобы
// обработала Matrix. Двухфазный диспетчер этого избегает.
//
// Если ни один лист не попал И ни одна цепочка вверх не нашла предка,
// содержащего точку, — запрос считается потерянным.
void dispatch(const Formula* root, RequestType t, int x, int y) {
    std::vector<const Formula*> leaves;
    collectLeaves(root, leaves);

    std::cout << "\n[" << Formula::requestName(t) << " " << x << "," << y
              << "] рассылка запроса от " << leaves.size() << " листьев:\n";

    // ФАЗА 1.
    for (const Formula* leaf : leaves) {
        if (leaf->contains(x, y)) {
            std::cout << "  лист " << leaf->typeName()
                      << " содержит точку — идём по цепочке вверх:\n";
            leaf->bubbleUp(t, x, y);
            return;
        }
    }
    std::cout << "  ни один лист не содержит точку — поднимаемся по parent_\n";

    // ФАЗА 2.
    const Formula* best = nullptr;
    int bestDepth = -1;
    for (const Formula* leaf : leaves) {
        const Formula* h = leaf->parent() ? leaf->parent()->findHandler(x, y) : nullptr;
        if (h) {
            int d = depthOf(h);
            if (d > bestDepth) {
                best = h;
                bestDepth = d;
            }
        }
    }

    if (best) {
        std::cout << "  самый глубокий контейнер, содержащий точку: "
                  << best->typeName() << " (глубина=" << bestDepth << ")\n";
        best->bubbleUp(t, x, y);
    } else {
        std::cout << "  никто не содержит точку — запрос потерян\n";
    }
}


// ---------------------------------------------------------------------------
//  printBoxes — для отладки: вывести дерево с bbox каждой формулы.
// ---------------------------------------------------------------------------
void printBoxes(const Formula* node, int indent = 0) {
    for (int i = 0; i < indent; ++i) std::cout << "  ";
    std::cout << node->describe() << "\n";
    for (auto k : node->children()) printBoxes(k, indent + 1);
}


// ---------------------------------------------------------------------------
//  writeSvg — записать дерево в SVG-файл (то же, что в Практике 1).
// ---------------------------------------------------------------------------
// ВАЖНО: при вызове root.draw() все формулы дерева попутно записывают свои
// абсолютные координаты в свои поля x_/y_/w_/h_. Без этого contains() не
// сможет работать.
void writeSvg(const Formula& root, const std::string& path, double margin = 30.0) {
    Box b = root.measure();
    double W = b.w + 2 * margin;
    double H = b.h + 2 * margin;
    std::ofstream f(path);
    f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    f << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << W
      << "\" height=\"" << H << "\" viewBox=\"0 0 " << W << " " << H << "\">\n";
    f << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
    root.draw(f, margin, margin);
    f << "</svg>\n";
}


// ===========================================================================
//  main — собирает формулу и запускает интерактивный цикл запросов
// ===========================================================================
int main() {
    // Переключаем консоль в UTF-8 для русских букв.
    std::system("chcp 65001 > nul");

    // ----- 1. Собираем дерево фигур -----
    // Дробь[ Интеграл(x, dx) / Матрица 3x3 ]. Все bbox честные, никакого
    // масштабирования.
    auto integral = std::make_unique<Integral>(
        std::make_unique<Atom>("x"),
        std::make_unique<Atom>("dx")
    );

    auto matrix = std::make_unique<Matrix>();
    const char* labels[3] = {"a", "b", "c"};
    for (int r = 0; r < 3; ++r)
        for (int c = 0; c < 3; ++c)
            matrix->setCell(r, c, std::make_unique<Atom>(labels[c]));

    auto root = std::make_unique<Fraction>(
        std::move(integral),
        std::move(matrix)
    );
    // На этом этапе все parent_ уже расставлены: setParent зовётся в
    // конструкторах Fraction/Integral и в Matrix::setCell.

    // ----- 2. Рисуем SVG (тут же все формулы запоминают свои bbox) -----
    writeSvg(*root, "chain.svg");

    // ----- 3. Чистим лог печати -----
    std::ofstream clearLog("print.log", std::ios::trunc);
    clearLog.close();

    std::cout << "SVG записан в chain.svg\n";
    std::cout << "\nДерево фигур и их bbox (после draw):\n";
    printBoxes(root.get());

    // ----- 4. Открываем картинку в браузере -----
    std::system("start \"\" chain.svg");

    // ----- 5. Меню запросов -----
    std::cout << "\n=== Цепочка обязанностей (parent_ + рассылка с листьев) ===\n";
    std::cout << "Команды: 'p X Y' — Print, 'h X Y' — Help, 'q' — выход.\n";
    std::cout << "Подсказка: координаты bbox см. выше.\n";

    char cmd;
    int x, y;
    while (std::cin >> cmd) {
        if (cmd == 'q') break;
        if (cmd == 'p' || cmd == 'h') {
            if (!(std::cin >> x >> y)) {
                std::cin.clear();
                std::string trash;
                std::getline(std::cin, trash);
                std::cout << "Нужны два целых числа.\n";
                continue;
            }
            // Диспетчеризация по типу запроса.
            RequestType t = (cmd == 'p') ? RequestType::Print : RequestType::Help;
            // Запускаем диспетчер. Он сам рассылает по листьям и поднимается
            // по parent_ в нужный момент.
            dispatch(root.get(), t, x, y);
        } else {
            std::cout << "Неизвестная команда. Используй p / h / q.\n";
        }
    }

    return 0;
}
