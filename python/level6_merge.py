"""Per przypadek wybiera najkrotszy poprawny program z outputs i candidates/*."""

import argparse
from collections import Counter
from pathlib import Path

from l6_core import CANDIDATE_DIR, NAMES, OUTPUT_DIR, baseline, load_cases, output_name, read_programs, write_programs


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", default=NAMES)
    parser.add_argument("--dry-run", action="store_true")
    parser.add_argument("--extra", nargs="*", default=[], type=Path,
                        help="dodatkowe katalogi z plikami out_level-6_*.txt (np. ../cpp/outputs)")
    args = parser.parse_args()
    dirs = sorted(p for p in CANDIDATE_DIR.iterdir() if p.is_dir()) if CANDIDATE_DIR.exists() else []
    dirs += [d for d in args.extra if d.is_dir()]
    total_old = total_new = 0
    for name in args.names:
        cases = load_cases(name)
        best = baseline(name, cases)
        old = sum(len(p) for p in best)
        wins = Counter()
        sources = {str(d): read_programs(d / output_name(name), len(cases)) for d in dirs}
        for i, case in enumerate(cases):
            for strategy, programs in sources.items():
                prog = programs[i]
                if prog is None:
                    continue
                prog = case.finish(prog)
                if prog is not None and len(prog) < len(best[i]):
                    best[i] = prog
                    wins[strategy] += 1
            assert case.finish(best[i]) is not None
        new = sum(len(p) for p in best)
        total_old += old
        total_new += new
        print(f"{name}: {old} -> {new}  {dict(wins)}")
        if not args.dry_run:
            write_programs(OUTPUT_DIR / output_name(name), best)
    print(f"TOTAL: {total_old} -> {total_new}")


if __name__ == "__main__":
    main()
