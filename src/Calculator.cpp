// ============================================================================
// Calculator.cpp — оркестрация расчёта ARL.
//
// Ключевые механики:
//  * Сетка (lambda, L) перебирается вложенными циклами по отсортированным
//    lambda_values / L_values. Каждая пара уникально идентифицируется uint64-ключом
//    (makeKey), что позволяет быстро пропускать уже посчитанные комбинации.
//  * Прогресс пишется построчно в temp_file сразу после расчёта пары
//    (flush на каждую строку) — это и есть главная защита от потери данных.
//  * Resume работает в два уровня: (1) completed_combinations_ из temp_file —
//    какие пары вообще посчитаны; (2) checkpoint_file — позиция последней
//    обработанной пары, чтобы не перебирать сетку с самого начала вхолостую.
//  * Проверка «isCombinationCompleted» всё равно важна в resume: если расчёт
//    упал между записью checkpoint и записью строки temp-файла, пара может
//    считаться «пройденной», но отсутствовать в файле.
// ============================================================================

#include "Calculator.hpp"
#include "Simulator.hpp"
#include "ProgressBar.hpp"
#include "Utils.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <thread>
#include <iomanip>

namespace ewma {

Calculator::Calculator(const Config& config) : config_(config) {
    results_.reserve(10000);
    completed_combinations_.reserve(10000);
}

// Упаковка двух double в один uint64-ключ:
//   lambda, L округляются до 3 знаков, сдвигаются на +10000 (чтобы уйти от
//   отрицательных хранимых значений для диапазонов lambda [0.05, 0.20] и
//   L [2.4, 3.0]) и умножаются на 1000. Старшие 32 бита ключа — lambda,
//   младшие 32 — L.
uint64_t Calculator::makeKey(double lambda, double L) const {
    uint32_t lambda_int = static_cast<uint32_t>(roundTo(lambda, 3) * 1000 + 10000);
    uint32_t L_int = static_cast<uint32_t>(roundTo(L, 3) * 1000 + 10000);
    return (static_cast<uint64_t>(lambda_int) << 32) | L_int;
}
// FIXME: +10000 и *1000 жёстко завязаны на диапазоны lambda [0.05, 0.20] и
// L [2.4, 3.0]. При расширении сетки ключ схлопнется (50019/50020 могут
// «столкнуться» в одну пару). Лучше строить ключ из канонической строки
// "lambda:L" или из битового представления double.

void Calculator::run(const Config& config) {
    config_ = config;
    
    std::ofstream tempFile;
    bool fileOpened = false;
    
    if (config_.resume) {
        loadCompletedCombinations();
        log("Resume mode: loaded " + std::to_string(completed_combinations_.size()) + " completed combinations");
        tempFile.open(config_.temp_file, std::ios::app);
        if (tempFile.is_open()) {
            fileOpened = true;
        }
    } else {
        completed_combinations_.clear();
        results_.clear();
        if (fileExists(config_.temp_file)) {
            deleteFile(config_.temp_file);
            log("Deleted old temp file for fresh start");
        }
        if (fileExists(config_.checkpoint_file)) {
            deleteFile(config_.checkpoint_file);
            log("Deleted old checkpoint file for fresh start");
        }
        tempFile.open(config_.temp_file);
        if (tempFile.is_open()) {
            fileOpened = true;
            tempFile << "lambda,L,ARL\n";
        }
        log("Fresh start: cleared all previous data");
    }
    
    if (!fileOpened) {
        log("ERROR: Cannot open temp file for writing");
        return;
    }
    
    ChartType chart = (config_.chart_type == "SR") ? ChartType::SR : ChartType::SN;
    
    auto lambda_values = config_.lambda_values;
    auto L_values = config_.L_values;
    
    std::sort(lambda_values.begin(), lambda_values.end());
    std::sort(L_values.begin(), L_values.end());
    
    int totalCombinations = lambda_values.size() * L_values.size();
    int completedCount = completed_combinations_.size();
    int remaining = totalCombinations - completedCount;
    
    log("Total combinations: " + std::to_string(totalCombinations));
    log("Already completed: " + std::to_string(completedCount));
    log("Remaining: " + std::to_string(remaining));
    
    if (remaining == 0) {
        log("All combinations already calculated.");
        tempFile.close();
        saveFinalResults();
        printTopResults(config_.top_n);
        return;
    }
    
    bool startProcessing = false;
    double lastLambda = 0, lastL = 0;
    
    if (config_.resume && fileExists(config_.checkpoint_file)) {
        loadCheckpoint(lastLambda, lastL);
        log("Resuming from checkpoint: lambda=" + std::to_string(lastLambda) + ", L=" + std::to_string(lastL));
    }
    
    ProgressBar progress(remaining, 50);
    auto startTime = std::chrono::steady_clock::now();
    processed_ = 0;
    
    bool hasBest = false;
    double bestLambda = 0.0, bestL = 0.0, bestARL = 0.0, bestDev = 1e9;
    bool earlyStopped = false;
    
    int updateCounter = 0;
    const int UPDATE_INTERVAL = config_.update_interval;
    
    for (double lambda : lambda_values) {
        if (earlyStopped) break;
        
        for (double L : L_values) {
            if (earlyStopped) break;
            
            // startProcessing: при resume пропускаем сетку до контрольной точки
            // (пары в checkpoint), чтобы не перебирать всё заново вхолостую.
            // В fresh-режиме всегда начинаем с первой пары.
            if (!startProcessing) {
                if (config_.resume && isClose(lambda, lastLambda) && isClose(L, lastL)) {
                    startProcessing = true;
                    log("Started processing from lambda=" + std::to_string(lambda) + ", L=" + std::to_string(L));
                }
                if (!config_.resume && lambda == lambda_values.front() && L == L_values.front()) {
                    startProcessing = true;
                }
                if (!startProcessing) continue;
            }
            
            if (isCombinationCompleted(lambda, L)) {
                continue;
            }
            
            processed_++;
            
            try {
                double ARL = calculateARLParallel(lambda, L, config_.simulations, config_.n, 
                                                  config_.max_iter, config_.n_cores, chart);
                
                tempFile << std::fixed << std::setprecision(6) 
                         << lambda << "," << L << "," << ARL << "\n";
                tempFile.flush();
                
                ResultEntry entry;
                entry.lambda = lambda;
                entry.L = L;
                entry.ARL = ARL;
                entry.key = makeKey(lambda, L);
                results_.push_back(entry);
                completed_combinations_.insert(entry.key);
                
                double dev = std::abs(ARL - config_.target_ARL);
                if (dev < bestDev) {
                    bestDev = dev;
                    bestLambda = lambda;
                    bestL = L;
                    bestARL = ARL;
                    hasBest = true;
                    progress.setBestResult(bestLambda, bestL, bestARL, config_.target_ARL);
                    
                    if (config_.tolerance >= 0 && bestDev <= config_.tolerance) {
                        earlyStopped = true;
                        progress.setEarlyStop(true);
                        log("Early stopping: found solution within tolerance " + 
                            std::to_string(config_.tolerance) + 
                            " (deviation: " + std::to_string(bestDev) + ")");
                        
                        tempFile.close();
                        saveFinalResults();
                        
                        auto endTime = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
                        progress.finish("Early stop! Time: " + formatTime(elapsed));
                        
                        log("Early stopped after " + std::to_string(processed_) + " combinations");
                        log("Best result: lambda=" + std::to_string(bestLambda) + ", L=" + std::to_string(bestL) + 
                            ", ARL=" + std::to_string(bestARL) + ", deviation=" + std::to_string(bestDev));
                        
                        printTopResults(config_.top_n);
                        return;
                    }
                }
                
                updateCounter++;
                if (updateCounter >= UPDATE_INTERVAL || processed_ == remaining) {
                    updateCounter = 0;
                    std::string msg = "lambda=" + std::to_string(lambda).substr(0, 5) + 
                                     ", L=" + std::to_string(L).substr(0, 4);
                    progress.update(processed_, msg, ARL);
                }
                
                if (processed_ % config_.checkpoint_interval == 0) {
                    saveCheckpoint(lambda, L);
                }
                
            } catch (const std::exception& e) {
                logError(lambda, L, e.what());
            }
        }
    }
    
    tempFile.close();
    progress.update(processed_, "Finalizing...", -1);
    
    auto endTime = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime).count();
    
    progress.finish("Complete! Time: " + formatTime(elapsed));
    
    log("Calculation completed: " + std::to_string(processed_) + " combinations");
    log("Total time: " + formatTime(elapsed));
    if (hasBest) {
        log("Best result: lambda=" + std::to_string(bestLambda) + ", L=" + std::to_string(bestL) + 
            ", ARL=" + std::to_string(bestARL) + ", deviation=" + std::to_string(bestDev));
    }
    
    saveFinalResults();
    printTopResults(config_.top_n);
}

void Calculator::loadCompletedCombinations() {
    completed_combinations_.clear();
    results_.clear();
    
    if (!fileExists(config_.temp_file)) {
        return;
    }
    
    std::ifstream file(config_.temp_file);
    if (file.peek() == std::ifstream::traits_type::eof()) {
        file.close();
        deleteFile(config_.temp_file);
        return;
    }
    
    file.clear();
    file.seekg(0);
    
    std::string line;
    int count = 0;
    bool firstLine = true;
    
    completed_combinations_.reserve(10000);
    results_.reserve(10000);
    
    while (std::getline(file, line)) {
        if (firstLine) {
            firstLine = false;
            continue;
        }
        
        auto parts = split(trim(line), ',');
        if (parts.size() >= 3) {
            try {
                double lambda = std::stod(parts[0]);
                double L = std::stod(parts[1]);
                double ARL = std::stod(parts[2]);
                
                ResultEntry entry;
                entry.lambda = lambda;
                entry.L = L;
                entry.ARL = ARL;
                entry.key = makeKey(lambda, L);
                
                completed_combinations_.insert(entry.key);
                results_.push_back(entry);
                count++;
            } catch (...) {
                // skip invalid lines
            }
        }
    }
    file.close();
    
    if (count > 0) {
        log("Loaded " + std::to_string(count) + " completed combinations from temp file");
    } else {
        deleteFile(config_.temp_file);
    }
}

bool Calculator::isCombinationCompleted(double lambda, double L) const {
    uint64_t key = makeKey(lambda, L);
    return completed_combinations_.find(key) != completed_combinations_.end();
}

void Calculator::saveCheckpoint(double lambda, double L) {
    std::ofstream file(config_.checkpoint_file);
    if (file.is_open()) {
        file << std::fixed << std::setprecision(6) << lambda << "," << L << "\n";
        file.close();
    }
}

void Calculator::loadCheckpoint(double& lambda, double& L) {
    if (fileExists(config_.checkpoint_file)) {
        std::string content = readFile(config_.checkpoint_file);
        auto parts = split(trim(content), ',');
        if (parts.size() >= 2) {
            lambda = std::stod(parts[0]);
            L = std::stod(parts[1]);
        }
    }
}

void Calculator::saveFinalResults() {
    if (results_.empty()) {
        log("No results to save");
        return;
    }
    
    if (fileExists(config_.temp_file)) {
        std::ifstream src(config_.temp_file);
        std::ofstream dst(config_.final_file);
        if (src.is_open() && dst.is_open()) {
            dst << src.rdbuf();
            dst.close();
            src.close();
            log("Copied temp file to " + config_.final_file);
            saveBestPairs();
            return;
        }
    }
    
    std::ofstream file(config_.final_file);
    if (file.is_open()) {
        file << "lambda,L,ARL\n";
        for (const auto& r : results_) {
            file << std::fixed << std::setprecision(6) 
                 << r.lambda << "," << r.L << "," << r.ARL << "\n";
        }
        file.close();
        log("Saved " + std::to_string(results_.size()) + " results to " + config_.final_file);
    }
    
    saveBestPairs();
}

void Calculator::saveBestPairs() {
    if (results_.empty()) return;
    
    std::vector<std::tuple<double, double, double, double>> bestPairs;
    bestPairs.reserve(results_.size());
    
    for (const auto& r : results_) {
        double dev = std::abs(r.ARL - config_.target_ARL);
        bestPairs.push_back({r.lambda, r.L, r.ARL, dev});
    }
    
    int n = std::min(config_.top_n, static_cast<int>(bestPairs.size()));
    
    std::nth_element(bestPairs.begin(), 
                     bestPairs.begin() + n,
                     bestPairs.end(),
                     [](const auto& a, const auto& b) {
                         return std::get<3>(a) < std::get<3>(b);
                     });
    
    std::sort(bestPairs.begin(), bestPairs.begin() + n,
              [](const auto& a, const auto& b) {
                  return std::get<3>(a) < std::get<3>(b);
              });
    
    std::ofstream bestFile(config_.best_file);
    if (bestFile.is_open()) {
        bestFile << "lambda,L,ARL,Deviation\n";
        for (int i = 0; i < n; ++i) {
            const auto& r = bestPairs[i];
            bestFile << std::fixed << std::setprecision(6)
                     << std::get<0>(r) << "," 
                     << std::get<1>(r) << "," 
                     << std::get<2>(r) << "," 
                     << std::get<3>(r) << "\n";
        }
        bestFile.close();
    }
}

void Calculator::printTopResults(int n) const {
    if (results_.empty()) {
        std::cout << "\nNo results available.\n";
        return;
    }
    
    std::vector<std::tuple<double, double, double, double>> withDeviation;
    withDeviation.reserve(results_.size());
    
    for (const auto& r : results_) {
        double dev = std::abs(r.ARL - config_.target_ARL);
        withDeviation.push_back({r.lambda, r.L, r.ARL, dev});
    }
    
    int count = std::min(n, static_cast<int>(withDeviation.size()));
    
    std::nth_element(withDeviation.begin(), 
                     withDeviation.begin() + count,
                     withDeviation.end(),
                     [](const auto& a, const auto& b) {
                         return std::get<3>(a) < std::get<3>(b);
                     });
    
    std::sort(withDeviation.begin(), withDeviation.begin() + count,
              [](const auto& a, const auto& b) {
                  return std::get<3>(a) < std::get<3>(b);
              });
    
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "BEST PAIR (lambda, L):\n";
    std::cout << std::string(70, '=') << "\n";
    const auto& best = withDeviation[0];
    std::cout << "  lambda = " << std::get<0>(best) << "\n";
    std::cout << "  L = " << std::get<1>(best) << "\n";
    std::cout << "  ARL = " << std::get<2>(best) << " (deviation: " << std::get<3>(best) << ")\n";
    std::cout << std::string(70, '=') << "\n";
    
    std::cout << "\nTOP " << n << " BEST PAIRS:\n";
    std::cout << std::string(70, '=') << "\n";
    std::cout << "  # |  lambda  |    L    |    ARL    | Deviation\n";
    std::cout << std::string(70, '-') << "\n";
    
    for (int i = 0; i < count; ++i) {
        const auto& r = withDeviation[i];
        printf(" %2d | %7.3f | %7.3f | %9.2f | %9.2f\n",
               i+1, std::get<0>(r), std::get<1>(r), std::get<2>(r), std::get<3>(r));
    }
    std::cout << std::string(70, '=') << "\n";
}

void Calculator::log(const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&time);
    timestamp.pop_back();
    
    std::ofstream file(config_.log_file, std::ios::app);
    if (file.is_open()) {
        file << "[" << timestamp << "] " << message << "\n";
        file.close();
    }
}

void Calculator::logError(double lambda, double L, const std::string& error) {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::string timestamp = std::ctime(&time);
    timestamp.pop_back();
    
    std::ofstream file(config_.error_file, std::ios::app);
    if (file.is_open()) {
        file << "[" << timestamp << "] lambda=" << lambda << ", L=" << L << ", error=" << error << "\n";
        file.close();
    }
}

void Calculator::resume(const Config& config) {
    config_ = config;
    run(config);
}

const std::vector<ResultEntry>& Calculator::getResults() const {
    return results_;
}

} // namespace ewma