#pragma once

#include <vector>
#include <unordered_set>
#include <tuple>
#include <string>
#include <cstdint>
#include "Config.hpp"

namespace ewma {

/// Одна запись результата: пара параметров (lambda, L) и полученный ARL.
struct ResultEntry {
    double lambda; ///< Константа сглаживания EWMA.
    double L;      ///< Множитель контрольного предела.
    double ARL;    ///< Средняя длина серии по прогонам Монте-Карло.
    uint64_t key;  ///< Уникальный ключ пары (см. makeKey).
};

///
/// Оркестратор расчёта ARL.
///
/// Перебирает сетку (lambda, L), вызывает симулятор, ведёт чекпоинты и
/// сохраняет результаты в CSV. Поддерживает прерывание и продолжение расчёта.
///
class Calculator {
public:
    /// Создаёт калькулятор для заданной конфигурации.
    explicit Calculator(const Config& config);

    /// Запускает перебор всех пар (lambda, L) из конфигурации.
    /// В resume-режиме предварительно загружает завершённые комбинации.
    void run(const Config& config);
    /// Продолжает расчёт с контрольной точки (обёртка над run с resume=true).
    void resume(const Config& config);

    /// Возвращает все накопленные результаты (для внешних API: bridge, Python).
    const std::vector<ResultEntry>& getResults() const;

private:
    /// Проверяет, была ли пара (lambda, L) уже посчитана (по ключу).
    bool isCombinationCompleted(double lambda, double L) const;
    /// Сохраняет контрольную точку: последнюю обработанную пару lambda,L.
    void saveCheckpoint(double lambda, double L);
    /// Загружает контрольную точку (пару lambda,L) из checkpoint_file.
    void loadCheckpoint(double& lambda, double& L);
    /// Читает завершённые комбинации из temp_file в completed_combinations_ и results_.
    void loadCompletedCombinations();
    /// Копирует temp_file в final_file и сохраняет лучшие пары.
    void saveFinalResults();
    /// Сохраняет топ-N пар по минимальному отклонению |ARL - target_ARL|.
    void saveBestPairs();
    /// Печатает топ-N лучших пар в stdout.
    void printTopResults(int n) const;
    /// Пишет построчную запись в log_file с временной меткой.
    void log(const std::string& message);
    /// Пишет ошибку расчёта пары (lambda, L) в error_file.
    void logError(double lambda, double L, const std::string& error);
    /// Строит uint64-ключ из (lambda, L): старшие 32 бита - lambda, младшие 32 бита - L.
    /// Смещение +10000 гарантирует неотрицательность хранения для
    /// lambda из [0.05, 0.20] и L из [2.4, 3.0].
    uint64_t makeKey(double lambda, double L) const;

    Config config_;
    /// Множество ключей уже посчитанных пар (для resume / пропуска дубликатов).
    std::unordered_set<uint64_t> completed_combinations_;
    /// Все накопленные результаты за текущий запуск.
    std::vector<ResultEntry> results_;
    /// Счётчик обработанных комбинаций (для прогресса и чекпоинтов).
    int processed_ = 0;
};

} // namespace ewma