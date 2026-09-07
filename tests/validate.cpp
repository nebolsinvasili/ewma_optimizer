// ============================================================================
// validate.cpp — валидация симулятора EWMA по книге Chakraborti & Graham,
// "Nonparametric Statistical Process Control" (гл. 4.2.3, EWMA-SN и EWMA-SR).
//
// Метод. Книга даёт для EWMA-карт два дизайн-пойнта, полученных Монте-Карло
// (100 000 прогонов, приложение B, SAS-программы 5 и 6):
//   * EWMA-SN (n=1, lambda=0.10, L=2.667): ARL_IC ≈ 481.94 (рис. 4.7,
//     номинал 500);
//   * EWMA-SR (n=10, lambda=0.10, L=2.794): ARL_IC ≈ 484.34 (рис. 4.8,
//     номинал 500).
// Валидация сверяет Монте-Карло симулятора проекта ПРОТИВ ЭТИХ КНИЖНЫХ
// ЗНАЧЕНИЙ (допуск ±5%), а также проверяет воспроизводимость: два независимых
// прогона Монте-Карло не должны расходиться более чем на ±2%.
//
// Запуск:  make validate   (по умолчанию 10000 прогонов Монте-Карло на точку)
//   либо:  ./tests/validate --simulations <N>
// Возврат: 0 — все проверки сошлись, 1 — есть провалы.
// ============================================================================

#include "Simulator.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

// Книжная дизайн-точка: параметры карты и эталонный ARL_IC из книги.
struct Case {
    const char* name;          ///< Описание.
    ewma::ChartType chart;     ///< Тип карты (SN или SR).
    double lambda;             ///< Константа сглаживания.
    double L;                  ///< Множитель контрольного предела.
    int n;                     ///< Размер подгруппы.
    double bookArl;            ///< Эталонный ARL_IC (книга, рис. 4.7/4.8).
};

const int kDefaultSims = 10000;
const int kMaxIter = 10000000;

const double kBookTol = 0.05;   // допуск MC к книжному значению
const double kReproTol = 0.02;  // допуск воспроизводимости между двумя прогонами

} // namespace

int main(int argc, char** argv) {
    int simulations = kDefaultSims;
    if (argc >= 3 && std::string(argv[1]) == "--simulations") {
        simulations = std::stoi(argv[2]);
    }

    const Case cases[] = {
        { "EWMA-SN  n=1  lambda=0.10 L=2.667",
          ewma::ChartType::SN, 0.10, 2.667, 1, 481.94 },
        { "EWMA-SR  n=10 lambda=0.10 L=2.794",
          ewma::ChartType::SR, 0.10, 2.794, 10, 484.34 },
    };

    std::cout << "EWMA validation vs book Monte-Carlo design points "
              << "(Chakraborti & Graham, ch. 4.2.3)\n"
              << "  Monte-Carlo: " << simulations << " runs/case\n"
              << "  book tolerance: " << (kBookTol * 100) << "%\n"
              << "  reproducibility tolerance: " << (kReproTol * 100) << "%\n\n";

    bool allOk = true;

    std::cout << "== 1. Monte-Carlo must reproduce book design points (ARL_IC ~ 500) ==\n";
    for (const auto& c : cases) {
        double arl = ewma::calculateARLParallel(c.lambda, c.L, simulations,
                                                c.n, kMaxIter, 0, c.chart);
        double dev = std::abs(arl - c.bookArl) / c.bookArl * 100.0;
        bool pass = dev <= kBookTol * 100.0;
        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] " << c.name
                  << "  MC=" << std::fixed << std::setprecision(1) << arl
                  << "  book=" << std::setprecision(1) << c.bookArl
                  << "  dev=" << std::setprecision(2) << dev << "%\n";
        allOk = allOk && pass;
    }

    std::cout << "\n== 2. Book design points close to nominal ARL_IC = 500 ==\n";
    for (const auto& c : cases) {
        double dev = std::abs(c.bookArl - 500.0) / 500.0 * 100.0;
        bool pass = dev <= 10.0;
        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] " << c.name
                  << ": book=" << std::fixed << std::setprecision(1) << c.bookArl
                  << " vs nominal 500, dev=" << std::setprecision(2) << dev
                  << "% (tol 10%)\n";
        allOk = allOk && pass;
    }

    std::cout << "\n== 3. Reproducibility: two independent MC runs must agree ==\n";
    for (const auto& c : cases) {
        // Разное число ядер даёт разные зерна (seed зависит от индекса ядра),
        // поэтому это две независимые Монте-Карло выборки одного ARL.
        double arl1 = ewma::calculateARLParallel(c.lambda, c.L, simulations,
                                                 c.n, kMaxIter, 1, c.chart);
        double arl2 = ewma::calculateARLParallel(c.lambda, c.L, simulations,
                                                 c.n, kMaxIter, 2, c.chart);
        double dev = std::abs(arl1 - arl2) / std::max(arl1, arl2) * 100.0;
        bool pass = dev <= kReproTol * 100.0;
        std::cout << "  [" << (pass ? "PASS" : "FAIL") << "] " << c.name
                  << "  run1=" << std::fixed << std::setprecision(1) << arl1
                  << "  run2=" << std::setprecision(1) << arl2
                  << "  dev=" << std::setprecision(2) << dev << "%\n";
        allOk = allOk && pass;
    }

    std::cout << "\n" << (allOk ? "ALL VALIDATION CHECKS PASSED"
                                : "VALIDATION FAILED")
              << std::endl;
    return allOk ? 0 : 1;
}