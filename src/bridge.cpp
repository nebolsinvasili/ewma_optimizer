#include "bridge.h"
#include "Config.hpp"
#include "Calculator.hpp"
#include "Simulator.hpp"
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>

static ewma::ChartType to_chart(EwmaChartType c) {
    return (c == EWMA_SR) ? ewma::ChartType::SR : ewma::ChartType::SN;
}

extern "C" {

EwmaConfig ewma_config_create(void) {
    auto* cfg = new(std::nothrow) ewma::Config();
    return static_cast<EwmaConfig>(cfg);
}

void ewma_config_destroy(EwmaConfig cfg) {
    if (cfg) {
        delete static_cast<ewma::Config*>(cfg);
    }
}

int ewma_config_from_json(EwmaConfig cfg, const char* json_str) {
    if (!cfg || !json_str) return -1;
    const char* tmp = "/tmp/ewma_config_tmp.json";
    FILE* f = std::fopen(tmp, "w");
    if (!f) return -1;
    std::fputs(json_str, f);
    std::fclose(f);
    try {
        *static_cast<ewma::Config*>(cfg) = ewma::Config::loadFromFile(tmp);
        std::remove(tmp);
        return 0;
    } catch (...) {
        std::remove(tmp);
        return -1;
    }
}

int ewma_config_set_simulations(EwmaConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->simulations = value;
    return 0;
}

int ewma_config_set_n(EwmaConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->n = value;
    return 0;
}

int ewma_config_set_max_iter(EwmaConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->max_iter = value;
    return 0;
}

int ewma_config_set_target_arl(EwmaConfig cfg, double value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->target_ARL = value;
    return 0;
}

int ewma_config_set_n_cores(EwmaConfig cfg, int value) {
    if (!cfg) return -1;
    static_cast<ewma::Config*>(cfg)->n_cores = value;
    return 0;
}

int ewma_config_set_tolerance(EwmaConfig cfg, double value) {
    if (!cfg) return -1;
    static_cast<ewma::Config*>(cfg)->tolerance = value;
    return 0;
}

int ewma_config_set_chart_type(EwmaConfig cfg, EwmaChartType chart) {
    if (!cfg) return -1;
    static_cast<ewma::Config*>(cfg)->chart_type = (chart == EWMA_SR) ? "SR" : "SN";
    return 0;
}

int ewma_config_set_lambda_range(EwmaConfig cfg, double start, double end, double step) {
    if (!cfg || step <= 0 || start > end) return -1;
    auto* c = static_cast<ewma::Config*>(cfg);
    c->lambda_values.clear();
    for (double v = start; v <= end + step * 0.5; v += step) {
        c->lambda_values.push_back(v);
    }
    return 0;
}

int ewma_config_set_l_range(EwmaConfig cfg, double start, double end, double step) {
    if (!cfg || step <= 0 || start > end) return -1;
    auto* c = static_cast<ewma::Config*>(cfg);
    c->L_values.clear();
    for (double v = start; v <= end + step * 0.5; v += step) {
        c->L_values.push_back(v);
    }
    return 0;
}

int ewma_config_set_top_n(EwmaConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->top_n = value;
    return 0;
}

int ewma_config_set_temp_file(EwmaConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<ewma::Config*>(cfg)->temp_file = path;
    return 0;
}

int ewma_config_set_checkpoint_file(EwmaConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<ewma::Config*>(cfg)->checkpoint_file = path;
    return 0;
}

int ewma_config_set_final_file(EwmaConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<ewma::Config*>(cfg)->final_file = path;
    return 0;
}

int ewma_config_set_best_file(EwmaConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<ewma::Config*>(cfg)->best_file = path;
    return 0;
}

int ewma_config_set_log_file(EwmaConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<ewma::Config*>(cfg)->log_file = path;
    return 0;
}

int ewma_config_set_error_file(EwmaConfig cfg, const char* path) {
    if (!cfg || !path) return -1;
    static_cast<ewma::Config*>(cfg)->error_file = path;
    return 0;
}

int ewma_config_set_checkpoint_interval(EwmaConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->checkpoint_interval = value;
    return 0;
}

int ewma_config_set_update_interval(EwmaConfig cfg, int value) {
    if (!cfg || value <= 0) return -1;
    static_cast<ewma::Config*>(cfg)->update_interval = value;
    return 0;
}

double ewma_calculate_arl(EwmaConfig cfg, double lambda, double L, EwmaChartType chart) {
    if (!cfg) return -1.0;
    auto* c = static_cast<ewma::Config*>(cfg);
    return ewma::calculateARLParallel(lambda, L, c->simulations, c->n,
                                      c->max_iter, c->n_cores, to_chart(chart));
}

double ewma_calculate_arl_dist(EwmaConfig cfg, double lambda, double L,
                               EwmaChartType chart, double* stats_out) {
    if (!cfg || !stats_out) return -1.0;
    auto* c = static_cast<ewma::Config*>(cfg);
    auto [arl, lengths, stats] = ewma::calculateARLParallelWithDistribution(
        lambda, L, c->simulations, c->n, c->max_iter, c->n_cores, to_chart(chart));
    int count = std::min(static_cast<int>(stats.size()), 10);
    for (int i = 0; i < count; ++i) stats_out[i] = stats[i];
    return arl;
}

EwmaResultArray ewma_run_grid(EwmaConfig cfg) {
    EwmaResultArray empty = {nullptr, 0};
    if (!cfg) return empty;

    auto* c = static_cast<ewma::Config*>(cfg);
    c->resume = false;

    ewma::Calculator calculator(*c);
    calculator.run(*c);

    const auto& results = calculator.getResults();
    if (results.empty()) return empty;

    EwmaResultArray arr;
    arr.count = static_cast<int>(results.size());
    arr.results = static_cast<EwmaResult*>(std::malloc(arr.count * sizeof(EwmaResult)));
    if (!arr.results) return empty;

    for (int i = 0; i < arr.count; ++i) {
        arr.results[i].lambda = results[i].lambda;
        arr.results[i].L = results[i].L;
        arr.results[i].ARL = results[i].ARL;
        arr.results[i].deviation = std::abs(results[i].ARL - c->target_ARL);
    }
    return arr;
}

void ewma_free_results(EwmaResultArray arr) {
    if (arr.results) {
        std::free(arr.results);
    }
}

} // extern "C"