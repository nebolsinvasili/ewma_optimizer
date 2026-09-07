"""ewma — CLI entry point (python -m ewma).

Python-аналог C++ CLI: перебор сетки (lambda, L) и вывод лучшей пары.
"""
import argparse
import sys
import os
from typing import List, Optional, Tuple

from . import Ewma, Config, Result


def _parse_range(args: Optional[List[str]]) -> Optional[Tuple[float, float, float]]:
    """Разбирает 'start end [step]' в (start, end, step)."""
    if not args:
        return None
    if len(args) < 2 or len(args) > 3:
        raise argparse.ArgumentTypeError("ожидается 'start end [step]'")
    start = float(args[0])
    end = float(args[1])
    step = float(args[2]) if len(args) == 3 else 0.1
    return (start, end, step)


def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="ewma",
        description="Расчёт ARL EWMA-карт (SN/SR) и подбор (lambda, L).",
    )
    p.add_argument("--chart", choices=["SN", "SR"], default="SN",
                   help="Тип карты (по умолчанию SN)")
    p.add_argument("--simulations", type=int, default=5000,
                   help="Число прогонов Монте-Карло на пару (lambda, L)")
    p.add_argument("--n", type=int, default=14, help="Размер подгруппы")
    p.add_argument("--max_iter", type=int, default=5000,
                   help="Максимальная длина серии")
    p.add_argument("--target_ARL", type=float, default=370.0,
                   help="Целевой ARL")
    p.add_argument("--cores", type=int, default=0,
                   help="Число ядер CPU (0 = авто)")
    p.add_argument("--tolerance", type=float, default=-1.0,
                   help="Ранняя остановка при |ARL-target| <= tolerance (-1 = выкл)")
    p.add_argument("--top_n", type=int, default=10,
                   help="Сколько лучших пар выводить")
    p.add_argument("--lambda_start", nargs="+", metavar=("S", "E"),
                   help="Диапазон lambda: start end [step]")
    p.add_argument("--L_start", nargs="+", metavar=("S", "E"),
                   help="Диапазон L: start end [step]")
    p.add_argument("--config", type=str, default=None,
                   help="Путь к JSON-конфигу (опции CLI переопределяют его)")
    p.add_argument("--json", action="store_true",
                   help="Выводить результат в JSON на stdout")
    p.add_argument("--keep-files", action="store_true",
                   help="Не удалять CSV/log-файлы после запуска")
    return p


def main(argv: Optional[List[str]] = None) -> int:
    args = build_parser().parse_args(argv)

    config = Config(
        simulations=args.simulations,
        n=args.n,
        max_iter=args.max_iter,
        target_ARL=args.target_ARL,
        n_cores=args.cores,
        tolerance=args.tolerance,
        chart_type=args.chart,
        top_n=args.top_n,
    )

    lambda_range = _parse_range(args.lambda_start)
    L_range = _parse_range(args.L_start)
    if lambda_range is None:
        lambda_range = (0.05, 0.20, 0.05)
    if L_range is None:
        L_range = (2.4, 3.0, 0.05)
    config.lambda_range = lambda_range
    config.L_range = L_range

    if args.chart == "SR" and (args.lambda_start is None or args.L_start is None):
        sys.stderr.write(
            "Предупреждение: для SR рекомендуется задать --lambda_start/--L_start явно.\n"
        )

    if args.config:
        with open(args.config, "r") as f:
            json_text = f.read()
        ewma = Ewma()
        ewma.load_json(json_text)
        ewma._apply_config(config)  # CLI опции переопределяют JSON
    else:
        ewma = Ewma(config)

    results = ewma.run()

    if not results:
        sys.stderr.write("Нет результатов.\n")
        return 1

    best = min(results, key=lambda r: r.deviation)

    if args.json:
        import json as jsonlib
        print(jsonlib.dumps({
            "best": {
                "lambda": best.lambda_,
                "L": best.L,
                "ARL": best.ARL,
                "deviation": best.deviation,
            },
            "count": len(results),
        }, indent=2))
    else:
        print("=" * 60)
        print("BEST PAIR (lambda, L):")
        print("=" * 60)
        print(f"  lambda = {best.lambda_:.4f}")
        print(f"  L      = {best.L:.4f}")
        print(f"  ARL    = {best.ARL:.2f} (deviation: {best.deviation:.2f})")
        print("=" * 60)

        top = sorted(results, key=lambda r: r.deviation)[: args.top_n]
        print(f"\nTOP {len(top)} BEST PAIRS:")
        print(f"  {'#':>3} | {'lambda':>7} | {'L':>7} | {'ARL':>9} | {'Deviation':>9}")
        print("-" * 48)
        for i, r in enumerate(top, 1):
            print(f"  {i:>3} | {r.lambda_:7.4f} | {r.L:7.3f} | {r.ARL:9.2f} | {r.deviation:9.2f}")

    if not args.keep_files:
        cwd = os.getcwd()
        for fname in ["ewma_arl_calculation_temp.csv", "ewma_arl_checkpoint.txt",
                      "ewma_arl_results_final.csv", "ewma_best_arl_pairs.csv",
                      "ewma_arl_calculation.log", "ewma_errors.log"]:
            path = os.path.join(cwd, fname)
            if os.path.exists(path):
                try:
                    os.remove(path)
                except OSError:
                    pass

    return 0


if __name__ == "__main__":
    sys.exit(main())
