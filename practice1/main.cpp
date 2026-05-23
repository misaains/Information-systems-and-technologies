#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

struct Box {
    double w;
    double h;
    double baseline;
};

class Formula {
public:
    virtual ~Formula() = default;
    virtual Box measure() const = 0;
    virtual void draw(std::ostream& out, double x, double y) const = 0;
};

using FormulaPtr = std::unique_ptr<Formula>;

class Atom : public Formula {
    std::string text_;
    double fontSize_;

public:
    explicit Atom(std::string text, double fontSize = 22.0)
        : text_(std::move(text)), fontSize_(fontSize) {}

    Box measure() const override {
        double w = static_cast<double>(text_.size()) * fontSize_ * 0.55;
        double h = fontSize_ * 1.2;
        double baseline = fontSize_ * 0.95;
        return {w, h, baseline};
    }

    void draw(std::ostream& out, double x, double y) const override {
        Box b = measure();
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
        : top_(std::move(top)), bottom_(std::move(bottom)) {}

    Box measure() const override {
        Box t = top_->measure();
        Box b = bottom_->measure();
        double w = std::max(t.w, b.w) + 2 * SIDE;
        double h = t.h + 2 * GAP + b.h;
        double baseline = t.h + GAP;
        return {w, h, baseline};
    }

    void draw(std::ostream& out, double x, double y) const override {
        Box me = measure();
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
        : body_(std::move(body)), diff_(std::move(diff)) {}

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

class Indexed : public Formula {
    FormulaPtr base_;
    FormulaPtr sub_;
    static constexpr double SUB_SCALE = 0.65;
    static constexpr double DROP_FRAC = 0.45;

public:
    Indexed(FormulaPtr base, FormulaPtr sub)
        : base_(std::move(base)), sub_(std::move(sub)) {}

    Box measure() const override {
        Box b = base_->measure();
        Box s = sub_->measure();
        double sw = s.w * SUB_SCALE;
        double sh = s.h * SUB_SCALE;
        double subTop = b.h * (1.0 - DROP_FRAC);
        double w = b.w + sw;
        double h = std::max(b.h, subTop + sh);
        return {w, h, b.baseline};
    }

    void draw(std::ostream& out, double x, double y) const override {
        Box b = base_->measure();
        base_->draw(out, x, y);

        double subTop = b.h * (1.0 - DROP_FRAC);
        double sx = x + b.w;
        double sy = y + subTop;
        out << "<g transform=\"translate(" << sx << "," << sy
            << ") scale(" << SUB_SCALE << ")\">\n";
        sub_->draw(out, 0, 0);
        out << "</g>\n";
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
        cells_[r][c] = std::move(child);
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
    auto matrix = std::make_unique<Matrix>();
    const char* letters[3] = {"a", "b", "c"};
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            matrix->setCell(r, c, std::make_unique<Atom>(letters[c]));
        }
    }

    auto integral = std::make_unique<Integral>(
        std::make_unique<Atom>("x"),
        std::make_unique<Atom>("dx")
    );

    auto subscript = std::make_unique<Fraction>(
        std::move(integral),
        std::make_unique<Atom>("q")
    );

    auto root = std::make_unique<Indexed>(
        std::move(matrix),
        std::move(subscript)
    );

    const std::string path = "result.svg";
    writeSvg(*root, path);
    std::cout << "SVG saved to " << path << "\n";

    std::system("start \"\" result.svg");
    return 0;
}
