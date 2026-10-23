"""T5: polerowanie gotowych programow lokalnymi ruchami.

Ruchy: usuniecie calego ticka, usuniecie opu jednego emitera z dosunieciem
reszty jego sciezki, zamiana sasiednich opow emitera, zmiana opu. Ruch jest
przyjmowany, gdy program dalej daje req odbiorow i nie jest dluzszy
(ruchy boczne pozwalaja wyjsc z plaskich obszarow).
"""

import argparse
from multiprocessing import Pool
from random import Random

from l6_core import CANDIDATE_DIR, NAMES, baseline, load_cases, output_name, write_programs


def set_op(tick, e, op):
    return tick[:e] + op + tick[e + 1:]


def shift_left(prog, e, t):
    """Usuwa op emitera e w ticku t; dalsza czesc jego sciezki przesuwa sie o tick."""
    out = list(prog)
    for i in range(t, len(prog) - 1):
        out[i] = set_op(out[i], e, prog[i + 1][e])
    out[-1] = set_op(out[-1], e, "-")
    return out


def shift_right(prog, e, t):
    """Wstawia '-' dla emitera e w ticku t (ostatni op emitera wypada)."""
    out = list(prog)
    for i in range(len(prog) - 1, t, -1):
        out[i] = set_op(out[i], e, prog[i - 1][e])
    out[t] = set_op(out[t], e, "-")
    return out


def greedy_delete(case, prog):
    t = 0
    while t < len(prog):
        cand = case.finish(prog[:t] + prog[t + 1:])
        if cand is not None:
            prog = cand
        else:
            t += 1
    return prog


def polish(args):
    case, prog, iterations, seed = args
    rng = Random(seed)
    best = case.finish(prog)
    if best is None:
        return None
    start_len = len(best)
    best = greedy_delete(case, best)
    cur = best
    for _ in range(iterations):
        n = len(cur)
        r = rng.random()
        e = rng.randrange(6)
        t = rng.randrange(n)
        if r < 0.3:
            cand = shift_left(cur, e, t)
        elif r < 0.5:
            cand = shift_right(cur, e, t)
        elif r < 0.75 and t + 1 < n:
            cand = list(cur)
            cand[t] = set_op(cur[t], e, cur[t + 1][e])
            cand[t + 1] = set_op(cur[t + 1], e, cur[t][e])
        else:
            cand = list(cur)
            cand[t] = set_op(cur[t], e, rng.choice("FBT-"))
        cand = case.finish(cand)
        if cand is None or len(cand) > len(cur):
            continue
        cur = cand
        if rng.random() < 0.2:
            cur = greedy_delete(case, cur)
        if len(cur) < len(best):
            best = cur
    return best if len(best) < start_len else None


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", default=NAMES)
    parser.add_argument("--iterations", type=int, default=3000)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--jobs", type=int, default=16)
    parser.add_argument("--tag", default="")
    args = parser.parse_args()
    for name in args.names:
        cases = load_cases(name)
        base = baseline(name, cases)
        tasks = [(case, prog, args.iterations, args.seed * 1000 + i)
                 for i, (case, prog) in enumerate(zip(cases, base))]
        with Pool(args.jobs) as pool:
            results = pool.map(polish, tasks, chunksize=1)
        old = sum(len(p) for p in base)
        new = sum(len(r) if r else len(p) for r, p in zip(results, base))
        print(f"{name}: {old} -> {new} (poprawione {sum(1 for r in results if r)}/{len(cases)})", flush=True)
        write_programs(CANDIDATE_DIR / f"polish{args.tag}" / output_name(name), results)


if __name__ == "__main__":
    main()
