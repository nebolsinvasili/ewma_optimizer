# AGENTS.md

Standalone C++17 CLI (Monte-Carlo ARL calculator for two-sided EWMA control charts with sign SN and signed-rank SR statistics, per Chakraborti & Graham, ch. 4.2.3). Single executable, no git repo, no CI, no test framework wired in.

## Build & run
- Build: `make` (outputs `bin/ewma`). Debug: `make debug`.
- Common runs: `make run-config` (uses `config/default_config.json`), `make run-quick` (1000 sims, coarse grid).
- CLI: `./bin/ewma --chart SN|SR --simulations N --lambda_start s e step --L_start s e step --target_ARL X --resume`. Full option table + semantics in `docs/API.md`.
- Validation: `make validate` (alias `make test`) builds `tests/validate` from `tests/validate.cpp` + `build/Simulator.o` + `build/Utils.o` and checks the Monte-Carlo ARL against the **book Monte-Carlo design points** (Chakraborti & Graham fig. 4.7/4.8: EWMA-SN n=1 λ=0.10 L=2.667 → ~481.94; EWMA-SR n=10 λ=0.10 L=2.794 → ~484.34; tol ±5%) plus a reproducibility check (two independent MC runs within ±2%); non-zero exit on failure. Verify changes with `make validate && make`.

## Gotchas
- Requires g++ with C++17 + OpenMP and the **nlohmann/json** header (`-I/usr/include`). Build fails if the header isn't installed.
- `make clean-all` deletes **all** `*.log *.csv *.txt *.png *.pdf` in the repo root — results are written into the CWD, not a separate dir.
- `ResultManager` (`src/ResultManager.cpp`, `include/ResultManager.hpp`) is **not in the Makefile's `SOURCES`** — never compiled/linked (still in `namespace cusum`). If you add it, add it to `SOURCES` or remove it.
- Default (no-arg) run loads `config/default_config.json` unless CLI args are given; `resume` is force-set to `false` there.
- Fresh-start deletes stale `temp`/`checkpoint` files listed in the JSON config; `--resume` only works if the temp CSV still exists. Return code is always `0` regardless of errors.
- The default lambda/L grid `[0.05, 0.20] × [2.4, 3.0]` is sized for ARL_IC ≈ 500. EWMA signalling is `Z_t > UCL` or `Z_t < LCL` (strict, per book SAS programs) where `UCL = L·scale·√(λ/(2−λ))`, scale = √n for SN and `√(n(n+1)(2n+1)/6)` for SR. λ is the smoothing constant, L the limit multiplier — these replace the former CUSUM k/H.
- Monte-Carlo per-pair at defaults (5000 sims × 4×13 grid) is CPU-heavier than it looks per pair (EWMA ARLs ~500 ⇒ long runs); parallelized via OpenMP (`n_cores <= 0` = auto).
- `tests/validate` asserts MC ARL vs the book's MC design points (tol ±5%). Both book points are ≈500 nominal (dev ≤ 10% asserted). The EWMA control limits are the **steady-state** limits (equations 4.17/4.22) — exact time-varying limits (4.16/4.21) are not implemented.
- `RandomGenerator` is a **thread-local** singleton (src/Utils.cpp `getInstance()`): OpenMP cores must never share one RNG (race corrupts draws and biases ARL low). Each core gets its own deterministic seed in `calculateARLParallel`. Keep it thread-local in any RNG changes.

## Docs
- `README.md` (Russian) — the full manual. `docs/API.md` — CLI, JSON schema, CSV formats, error handling. Config schema lives in `config/default_config.json` and is duplicated in `Config` (include/Config.hpp).
- Source comments, README, and API.md are in Russian — match that for user-facing output.