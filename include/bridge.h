#ifndef EWMA_BRIDGE_H
#define EWMA_BRIDGE_H

#ifdef __cplusplus
extern "C" {
#endif

typedef enum { EWMA_SN = 0, EWMA_SR = 1 } EwmaChartType;

typedef struct {
    double lambda;
    double L;
    double ARL;
    double deviation;
} EwmaResult;

typedef struct {
    EwmaResult* results;
    int count;
} EwmaResultArray;

/* Opaque handle to Config */
typedef void* EwmaConfig;

/* Opaque handle to Calculator */
typedef void* EwmaCalculator;

/* Config lifecycle */
EwmaConfig ewma_config_create(void);
void ewma_config_destroy(EwmaConfig cfg);
int  ewma_config_from_json(EwmaConfig cfg, const char* json_str);

/* Config field setters (return 0 on success, -1 on bad input) */
int ewma_config_set_simulations(EwmaConfig cfg, int value);
int ewma_config_set_n(EwmaConfig cfg, int value);
int ewma_config_set_max_iter(EwmaConfig cfg, int value);
int ewma_config_set_target_arl(EwmaConfig cfg, double value);
int ewma_config_set_n_cores(EwmaConfig cfg, int value);
int ewma_config_set_tolerance(EwmaConfig cfg, double value);
int ewma_config_set_chart_type(EwmaConfig cfg, EwmaChartType chart);
int ewma_config_set_lambda_range(EwmaConfig cfg, double start, double end, double step);
int ewma_config_set_l_range(EwmaConfig cfg, double start, double end, double step);
int ewma_config_set_top_n(EwmaConfig cfg, int value);
int ewma_config_set_temp_file(EwmaConfig cfg, const char* path);
int ewma_config_set_checkpoint_file(EwmaConfig cfg, const char* path);
int ewma_config_set_final_file(EwmaConfig cfg, const char* path);
int ewma_config_set_best_file(EwmaConfig cfg, const char* path);
int ewma_config_set_log_file(EwmaConfig cfg, const char* path);
int ewma_config_set_error_file(EwmaConfig cfg, const char* path);
int ewma_config_set_checkpoint_interval(EwmaConfig cfg, int value);
int ewma_config_set_update_interval(EwmaConfig cfg, int value);

/* Single-pair ARL calculation */
double ewma_calculate_arl(EwmaConfig cfg, double lambda, double L, EwmaChartType chart);

/* Single-pair ARL with distribution stats.
   stats_out must point to array of at least 10 doubles.
   Returns ARL, fills stats_out with: [ARL, mean, stddev, p5, p25, p50, p75, p95, min, max]. */
double ewma_calculate_arl_dist(EwmaConfig cfg, double lambda, double L,
                               EwmaChartType chart, double* stats_out);

/* Full grid search — returns array of results. Caller must free with ewma_free_results. */
EwmaResultArray ewma_run_grid(EwmaConfig cfg);

/* Free result array returned by ewma_run_grid. */
void ewma_free_results(EwmaResultArray arr);

#ifdef __cplusplus
}
#endif

#endif /* EWMA_BRIDGE_H */