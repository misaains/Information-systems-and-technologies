#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>


struct Slice {
    std::string name;
    int percent;
};


static const std::string PALETTE[3] = {
    "#c0392b",  // красный
    "#2980b9",  // синий
    "#27ae60"   // зелёный
};


class IObserver {
public:
    virtual ~IObserver() = default;
    virtual void update() = 0;
};


class Model {
    std::vector<Slice> slices_;
    std::vector<IObserver*> observers_;

public:
    explicit Model(std::vector<Slice> initial)
        : slices_(std::move(initial)) {}

    void attach(IObserver* o) { observers_.push_back(o); }

    void detach(IObserver* o) {
        observers_.erase(std::remove(observers_.begin(), observers_.end(), o),
                         observers_.end());
    }

    const std::vector<Slice>& slices() const { return slices_; }

    bool setPercents(const std::vector<int>& percents) {
        if (percents.size() != slices_.size()) {
            std::cout << "Ошибка: ожидается " << slices_.size() << " чисел.\n";
            return false;
        }
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
        for (std::size_t i = 0; i < slices_.size(); ++i) {
            slices_[i].percent = percents[i];
        }
        notify();
        return true;
    }

    void notify() {
        for (IObserver* o : observers_) {
            o->update();
        }
    }
};


class IView : public IObserver {
public:
    void update() override { writeFile(); }

    virtual std::string filename() const = 0;
    virtual std::pair<double, double> canvasSize() const = 0;
    virtual std::string renderBody() const = 0;

private:
    void writeFile() const {
        auto [w, h] = canvasSize();
        std::ofstream f(filename());
        f << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
        f << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << w
          << "\" height=\"" << h << "\" viewBox=\"0 0 " << w << " " << h << "\">\n";
        f << "<rect width=\"100%\" height=\"100%\" fill=\"white\"/>\n";
        f << renderBody() << "\n";
        f << "</svg>\n";
    }
};


class TableView : public IView {
    const Model* model_;
    static constexpr double CELL_W = 110.0;
    static constexpr double CELL_H = 50.0;
    static constexpr double PAD = 15.0;

public:
    explicit TableView(const Model* m) : model_(m) {}

    std::string filename() const override { return "time_table.svg"; }

    std::pair<double, double> canvasSize() const override {
        double w = CELL_W * model_->slices().size() + 2 * PAD;
        double h = 2 * CELL_H + 2 * PAD;
        return {w, h};
    }

    std::string renderBody() const override {
        const auto& slices = model_->slices();
        std::ostringstream s;
        double x0 = PAD, y0 = PAD;
        double tableW = CELL_W * slices.size();
        double tableH = 2 * CELL_H;

        s << "<rect x=\"" << x0 << "\" y=\"" << y0
          << "\" width=\"" << tableW << "\" height=\"" << tableH
          << "\" fill=\"none\" stroke=\"black\" stroke-width=\"2\"/>\n";

        s << "<line x1=\"" << x0 << "\" y1=\"" << (y0 + CELL_H)
          << "\" x2=\"" << (x0 + tableW) << "\" y2=\"" << (y0 + CELL_H)
          << "\" stroke=\"black\" stroke-width=\"2\"/>\n";

        for (std::size_t i = 0; i < slices.size(); ++i) {
            double cx = x0 + i * CELL_W;
            if (i > 0) {
                s << "<line x1=\"" << cx << "\" y1=\"" << y0
                  << "\" x2=\"" << cx << "\" y2=\"" << (y0 + tableH)
                  << "\" stroke=\"black\" stroke-width=\"2\"/>\n";
            }
            s << "<text x=\"" << (cx + CELL_W / 2) << "\" y=\"" << (y0 + CELL_H * 0.65)
              << "\" font-size=\"22\" text-anchor=\"middle\" font-family=\"serif\">"
              << slices[i].name << "</text>\n";
            s << "<text x=\"" << (cx + CELL_W / 2) << "\" y=\"" << (y0 + CELL_H * 1.65)
              << "\" font-size=\"22\" text-anchor=\"middle\" font-family=\"serif\""
              << " fill=\"" << PALETTE[i % 3] << "\">"
              << slices[i].percent << "%</text>\n";
        }
        return s.str();
    }
};


class BarChartView : public IView {
    const Model* model_;
    static constexpr double MARGIN_L = 55.0;
    static constexpr double MARGIN_R = 25.0;
    static constexpr double MARGIN_T = 25.0;
    static constexpr double MARGIN_B = 55.0;
    static constexpr double BAR_W = 65.0;
    static constexpr double BAR_GAP = 30.0;
    static constexpr double MAX_PCT = 100.0;
    static constexpr double PLOT_H = 260.0;

public:
    explicit BarChartView(const Model* m) : model_(m) {}

    std::string filename() const override { return "time_bars.svg"; }

    std::pair<double, double> canvasSize() const override {
        std::size_t n = model_->slices().size();
        double plotW = n * BAR_W + (n + 1) * BAR_GAP;
        return {MARGIN_L + plotW + MARGIN_R, MARGIN_T + PLOT_H + MARGIN_B};
    }

    std::string renderBody() const override {
        const auto& slices = model_->slices();
        std::ostringstream s;
        std::size_t n = slices.size();
        double plotW = n * BAR_W + (n + 1) * BAR_GAP;
        double y0 = MARGIN_T + PLOT_H;

        for (int p = 0; p <= 100; p += 10) {
            double y = y0 - (p / MAX_PCT) * PLOT_H;
            s << "<line x1=\"" << MARGIN_L << "\" y1=\"" << y
              << "\" x2=\"" << (MARGIN_L + plotW) << "\" y2=\"" << y
              << "\" stroke=\"lightgray\" stroke-width=\"1\"/>\n";
            s << "<text x=\"" << (MARGIN_L - 8) << "\" y=\"" << (y + 5)
              << "\" font-size=\"14\" text-anchor=\"end\" font-family=\"sans-serif\">"
              << p << "%</text>\n";
        }

        s << "<line x1=\"" << MARGIN_L << "\" y1=\"" << MARGIN_T
          << "\" x2=\"" << MARGIN_L << "\" y2=\"" << y0
          << "\" stroke=\"black\" stroke-width=\"2\"/>\n";
        s << "<line x1=\"" << MARGIN_L << "\" y1=\"" << y0
          << "\" x2=\"" << (MARGIN_L + plotW) << "\" y2=\"" << y0
          << "\" stroke=\"black\" stroke-width=\"2\"/>\n";

        for (std::size_t i = 0; i < n; ++i) {
            double bx = MARGIN_L + BAR_GAP + i * (BAR_W + BAR_GAP);
            double bh = (slices[i].percent / MAX_PCT) * PLOT_H;
            double by = y0 - bh;
            s << "<rect x=\"" << bx << "\" y=\"" << by
              << "\" width=\"" << BAR_W << "\" height=\"" << bh
              << "\" fill=\"" << PALETTE[i % 3] << "\"/>\n";
            s << "<text x=\"" << (bx + BAR_W / 2) << "\" y=\"" << (y0 + 28)
              << "\" font-size=\"18\" text-anchor=\"middle\" font-family=\"serif\">"
              << slices[i].name << "</text>\n";
        }
        return s.str();
    }
};


class PieChartView : public IView {
    const Model* model_;
    static constexpr double CX = 200.0;
    static constexpr double CY = 200.0;
    static constexpr double R = 140.0;
    static constexpr double PI = 3.14159265358979323846;

public:
    explicit PieChartView(const Model* m) : model_(m) {}

    std::string filename() const override { return "time_pie.svg"; }

    std::pair<double, double> canvasSize() const override {
        return {400.0, 440.0};
    }

    std::string renderBody() const override {
        const auto& slices = model_->slices();
        const std::string* colors = PALETTE;  // красный / синий / зелёный
        std::ostringstream s;

        double startAng = -90.0;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            double sweep = slices[i].percent * 3.6;
            double endAng = startAng + sweep;
            double x1 = CX + R * std::cos(startAng * PI / 180.0);
            double y1 = CY + R * std::sin(startAng * PI / 180.0);
            double x2 = CX + R * std::cos(endAng * PI / 180.0);
            double y2 = CY + R * std::sin(endAng * PI / 180.0);
            int largeArc = (sweep > 180.0) ? 1 : 0;

            if (slices[i].percent == 100) {
                s << "<circle cx=\"" << CX << "\" cy=\"" << CY
                  << "\" r=\"" << R << "\" fill=\"" << colors[i % 3]
                  << "\" stroke=\"white\" stroke-width=\"2\"/>\n";
            } else if (slices[i].percent > 0) {
                s << "<path d=\"M " << CX << " " << CY
                  << " L " << x1 << " " << y1
                  << " A " << R << " " << R << " 0 " << largeArc << " 1 "
                  << x2 << " " << y2 << " Z\" "
                  << "fill=\"" << colors[i % 3] << "\" stroke=\"white\" stroke-width=\"2\"/>\n";
            }

            startAng = endAng;
        }

        double lx = 30, ly = 380;
        for (std::size_t i = 0; i < slices.size(); ++i) {
            double sx = lx + i * 120;
            s << "<rect x=\"" << sx << "\" y=\"" << ly
              << "\" width=\"14\" height=\"14\" fill=\"" << colors[i % 3] << "\"/>\n";
            s << "<text x=\"" << (sx + 20) << "\" y=\"" << (ly + 12)
              << "\" font-size=\"15\" font-family=\"serif\">"
              << slices[i].name << "</text>\n";
        }
        return s.str();
    }
};


class FrameDecorator : public IView {
    IView* inner_;
    static constexpr double PAD = 15.0;
    static constexpr double FRAME_INSET = 4.0;

public:
    explicit FrameDecorator(IView* inner) : inner_(inner) {}

    std::string filename() const override { return inner_->filename(); }

    std::pair<double, double> canvasSize() const override {
        auto [w, h] = inner_->canvasSize();
        return {w + 2 * PAD, h + 2 * PAD};
    }

    std::string renderBody() const override {
        auto [w, h] = canvasSize();
        std::ostringstream s;
        s << "<rect x=\"" << FRAME_INSET << "\" y=\"" << FRAME_INSET
          << "\" width=\"" << (w - 2 * FRAME_INSET)
          << "\" height=\"" << (h - 2 * FRAME_INSET)
          << "\" fill=\"none\" stroke=\"#4a76c5\" stroke-width=\"3\"/>\n";
        s << "<g transform=\"translate(" << PAD << "," << PAD << ")\">\n"
          << inner_->renderBody() << "\n</g>\n";
        return s.str();
    }
};


class Controller {
    Model* model_;
    std::mt19937 rng_;

public:
    explicit Controller(Model* m)
        : model_(m), rng_(std::random_device{}()) {}

    void run() {
        while (true) {
            std::cout << "\n=== Распределение времени (с рамкой) ===\n";
            std::cout << "1) ввести проценты вручную\n";
            std::cout << "2) случайные значения\n";
            std::cout << "0) выход\n";
            std::cout << "> ";

            int cmd;
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
        if (model_->setPercents(ps)) {
            std::cout << "Обновлено. Нажмите F5 в браузере.\n";
        }
    }

    void randomValues() {
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


int main() {
    std::system("chcp 65001 > nul");

    Model model({
        {"Учёба", 30},
        {"Сон",   35},
        {"Отдых", 35}
    });

    TableView    table(&model);
    BarChartView bars(&model);
    PieChartView pie(&model);

    FrameDecorator framedTable(&table);
    FrameDecorator framedBars(&bars);
    FrameDecorator framedPie(&pie);

    model.attach(&framedTable);
    model.attach(&framedBars);
    model.attach(&framedPie);

    model.notify();

    std::system("start \"\" time_table.svg");
    std::system("start \"\" time_bars.svg");
    std::system("start \"\" time_pie.svg");

    Controller c(&model);
    c.run();

    return 0;
}
