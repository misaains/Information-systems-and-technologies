#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>


struct Box {
    double w;
    double h;
    double baseline;
};


enum class RequestType { Print, Help };


class Formula {
protected:
    mutable double x_ = 0, y_ = 0, w_ = 0, h_ = 0;
    Formula* parent_ = nullptr;

public:
    virtual ~Formula() = default;

    virtual Box measure() const = 0;
    virtual void draw(std::ostream& out, double x, double y) const = 0;
    virtual std::string typeName() const = 0;
    virtual std::vector<const Formula*> children() const { return {}; }

    void setParent(Formula* p) { parent_ = p; }
    Formula* parent() const { return parent_; }

    bool contains(int px, int py) const {
        double x = static_cast<double>(px);
        double y = static_cast<double>(py);
        return x >= x_ && x < x_ + w_ && y >= y_ && y < y_ + h_;
    }

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

    const Formula* findHandler(int x, int y) const {
        if (contains(x, y)) return this;
        if (parent_) return parent_->findHandler(x, y);
        return nullptr;
    }

private:
    void doHandle(RequestType t, int x, int y) const {
        if (t == RequestType::Help) {
            std::cout << "  [Help " << x << "," << y << "] " << describe() << "\n";
        } else {
            std::ofstream f("print.log", std::ios::app);
            f << "[Print " << x << "," << y << "] " << describe() << "\n";
            std::cout << "  [Print " << x << "," << y
                      << "] записано в print.log: " << describe() << "\n";
        }
    }
};

using FormulaPtr = std::unique_ptr<Formula>;


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
        x_ = x; y_ = y; w_ = b.w; h_ = b.h;
        out << "<text x=\"" << x << "\" y=\"" << (y + b.baseline)
            << "\" font-family=\"Cambria Math, Times, serif\" font-style=\"italic\""
            << " font-size=\"" << fontSize_ << "\">" << text_ << "</text>\n";
    }
};


class Fraction : public Formula {
    FormulaPtr top_;
    FormulaPtr bottom_;
    static constexpr double GAP = 4.0;
    static constexpr double SIDE = 8.0;

public:
    Fraction(FormulaPtr top, FormulaPtr bottom)
        : top_(std::move(top)), bottom_(std::move(bottom)) {
        top_->setParent(this);
        bottom_->setParent(this);
    }

    std::string typeName() const override { return "Дробь"; }

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
        x_ = x; y_ = y; w_ = me.w; h_ = me.h;
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
    void setCell(int r, int c, FormulaPtr child) {
        child->setParent(this);
        cells_[r][c] = std::move(child);
    }

    std::string typeName() const override { return "Матрица 3x3"; }

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

        out << "<path d=\"M " << (x + BRACKET_W) << " " << y
            << " Q " << x << " " << (y + me.h / 2.0)
            << " " << (x + BRACKET_W) << " " << (y + me.h)
            << "\" fill=\"none\" stroke=\"black\" stroke-width=\"1.5\"/>\n";

        double rx = x + me.w;
        out << "<path d=\"M " << (rx - BRACKET_W) << " " << y
            << " Q " << rx << " " << (y + me.h / 2.0)
            << " " << (rx - BRACKET_W) << " " << (y + me.h)
            << "\" fill=\"none\" stroke=\"black\" stroke-width=\"1.5\"/>\n";

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


void collectLeaves(const Formula* node, std::vector<const Formula*>& out) {
    auto kids = node->children();
    if (kids.empty()) {
        out.push_back(node);
        return;
    }
    for (auto k : kids) collectLeaves(k, out);
}

int depthOf(const Formula* n) {
    int d = 0;
    while (n) { ++d; n = n->parent(); }
    return d;
}

void dispatch(const Formula* root, RequestType t, int x, int y) {
    std::vector<const Formula*> leaves;
    collectLeaves(root, leaves);

    std::cout << "\n[" << Formula::requestName(t) << " " << x << "," << y
              << "] рассылка запроса от " << leaves.size() << " листьев:\n";

    for (const Formula* leaf : leaves) {
        if (leaf->contains(x, y)) {
            std::cout << "  лист " << leaf->typeName()
                      << " содержит точку — идём по цепочке вверх:\n";
            leaf->bubbleUp(t, x, y);
            return;
        }
    }
    std::cout << "  ни один лист не содержит точку — поднимаемся по parent_\n";

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


void printBoxes(const Formula* node, int indent = 0) {
    for (int i = 0; i < indent; ++i) std::cout << "  ";
    std::cout << node->describe() << "\n";
    for (auto k : node->children()) printBoxes(k, indent + 1);
}


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


int main() {
    std::system("chcp 65001 > nul");

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

    writeSvg(*root, "chain.svg");
    std::ofstream clearLog("print.log", std::ios::trunc);
    clearLog.close();

    std::cout << "SVG записан в chain.svg\n";
    std::cout << "\nДерево фигур и их bbox (после draw):\n";
    printBoxes(root.get());

    std::system("start \"\" chain.svg");

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
            RequestType t = (cmd == 'p') ? RequestType::Print : RequestType::Help;
            dispatch(root.get(), t, x, y);
        } else {
            std::cout << "Неизвестная команда. Используй p / h / q.\n";
        }
    }

    return 0;
}
