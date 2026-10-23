"""T4: beam search po pelnym zbiorze akcji (do 4^6 kombinacji na tick).

Ocena stanu: odbiory, pokryte komorki wzoru, kara za materie w halo oraz
potencjal linii (ile brakujacych komorek / materii w halo lezy na liniach
emiterow), ktory prowadzi dojazdy bez natychmiastowego zysku.
Dzieci liczone przez dekompozycje: para osi (0,1) -> 256 kombinacji z
gotowymi maskami trafien, potem dokladana os 2 (16 kombinacji).
"""

import argparse
import heapq
from multiprocessing import Pool
from random import Random

from l6_core import CANDIDATE_DIR, NAMES, baseline, load_cases, output_name, write_programs

BIG = 10 ** 6


def emitter_options(case, e, pos, active):
    """(op, nowa pozycja, aktywny?, maska promienia jesli aktywny)."""
    L = case.L
    ray = case.ray[e]
    result = [("-", pos, active)]
    for op, step in (("F", 1), ("B", -1)):
        p = (pos + step) % L
        if ray[p]:
            result.append((op, p, active))
    result.append(("T", pos, not active))
    return [(op, p, a, ray[p] if a else 0) for op, p, a in result]


def axis_combos(o1, o2):
    """Kombinacje dwoch emiterow jednej osi: (opy, (one, two))."""
    out = []
    for a in o1:
        for b in o2:
            ma, mb = a[3], b[3]
            out.append(((a, b), ma ^ mb, ma & mb))
    return out


def expand(case, node, rng, keep, params):
    """Najlepsze dzieci wezla: lista (score, key, data)."""
    alpha, gamma, beta = params
    pos, active, matter, acc = node
    P, H, req = case.P, case.H, case.req
    missing = P & ~matter
    dirty = H & matter
    opts = [emitter_options(case, e, pos[e], active >> e & 1) for e in range(6)]
    # Potencjal liczony na stanie rodzica: wartosc linii, na ktorej stanie emiter.
    pots = []
    for e in range(6):
        ray = case.ray[e]
        pots.append([(ray[o[1]] & missing).bit_count() + beta * (ray[o[1]] & dirty).bit_count()
                     for o in opts[e]])
    ax = [axis_combos(opts[a], opts[a + 3]) for a in range(3)]
    axp = [[pots[a][i] + pots[a + 3][j] for i in range(len(opts[a])) for j in range(len(opts[a + 3]))]
           for a in range(3)]
    # Para osi 0 i 1: dokladnie 0/1/2 i >=3 trafien.
    pair = []
    for (ops0, o0, t0), p0 in zip(ax[0], axp[0]):
        u0 = o0 | t0
        for (ops1, o1, t1), p1 in zip(ax[1], axp[1]):
            u1 = o1 | t1
            s1 = (o0 ^ o1) & ~(t0 | t1)
            s2 = (t0 & ~u1) | (t1 & ~u0) | (o0 & o1)
            g3 = (t0 & u1) | (t1 & u0)
            pair.append((ops0 + ops1, s1, s2, g3, s1 | s2 | g3, p0 + p1))
    heap = []
    counter = 0
    third = [(c[0], c[1], c[2], c[1] | c[2], p) for c, p in zip(ax[2], axp[2])]
    for ops01, s1, s2, g3, any01, p01 in pair:
        for ops2, o2, t2, u2, p2 in third:
            exact2 = (s2 & ~u2) | (s1 & o2) | (t2 & ~any01)
            ge3 = g3 | (s2 & u2) | (s1 & t2)
            new = (matter | exact2) & ~ge3
            nacc = acc
            if new & P == P and not new & H:
                nacc += 1
                new &= ~P
            score = (nacc * BIG + (new & P).bit_count() * 4
                     - alpha * 4 * (new & H).bit_count() + gamma * (p01 + p2) + rng.random())
            item = (score, counter, ops01, ops2, new, nacc)
            counter += 1
            if len(heap) < keep:
                heapq.heappush(heap, item)
            elif score > heap[0][0]:
                heapq.heapreplace(heap, item)
    result = []
    for score, _, ops01, ops2, new, nacc in heap:
        # kolejnosc emiterow: os0 -> (0,3), os1 -> (1,4), os2 -> (2,5)
        by_e = [None] * 6
        by_e[0], by_e[3] = ops01[0], ops01[1]
        by_e[1], by_e[4] = ops01[2], ops01[3]
        by_e[2], by_e[5] = ops2[0], ops2[1]
        npos = tuple(o[1] for o in by_e)
        nact = sum(1 << e for e in range(6) if by_e[e][2])
        text = "".join(o[0] for o in by_e)
        result.append((score, (npos, nact, new, nacc), text))
    return result


def beam(case, width, params, limit, seed):
    rng = Random(seed)
    start = (tuple(case.home(e) for e in range(6)), 0, 0, 0)
    layer = [(0, start, None)]  # (score, node, (text, parent_layer_index))
    history = []
    for depth in range(limit):
        children = {}
        for idx, (_, node, _) in enumerate(layer):
            for score, child, text in expand(case, node, rng, width, params):
                old = children.get(child)
                if old is None or score > old[0]:
                    children[child] = (score, child, (text, idx))
        history.append(layer)
        layer = heapq.nlargest(width, children.values(), key=lambda x: x[0])
        for score, node, link in layer:
            if node[3] == case.req:
                prog = []
                text, idx = link
                prog.append(text)
                for prev in reversed(history[1:]):
                    _, _, (text, idx) = prev[idx]
                    prog.append(text)
                prog.reverse()
                return case.finish(prog)
    return None


def solve_case(args):
    case, base_len, width, settings, seed = args
    best = None
    limit = base_len - 1
    for i, params in enumerate(settings):
        prog = beam(case, width, params, limit, seed + i)
        if prog is not None and len(prog) <= limit:
            best = prog
            limit = len(prog) - 1
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", default=NAMES)
    parser.add_argument("--width", type=int, default=30)
    parser.add_argument("--settings", default="1.5:0.3:1",
                        help="lista alpha:gamma:beta oddzielona przecinkami")
    parser.add_argument("--cases", default="", help="np. 0-9 (indeksy przypadkow)")
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--jobs", type=int, default=16)
    parser.add_argument("--tag", default="")
    args = parser.parse_args()
    settings = [tuple(float(x) for x in s.split(":")) for s in args.settings.split(",")]
    for name in args.names:
        cases = load_cases(name)
        base = baseline(name, cases)
        todo = list(range(len(cases)))
        if args.cases:
            lo, hi = map(int, args.cases.split("-"))
            todo = [i for i in todo if lo <= i <= hi]
        tasks = [(cases[i], len(base[i]), args.width, settings, args.seed * 1000 + i) for i in todo]
        with Pool(args.jobs) as pool:
            found = pool.map(solve_case, tasks, chunksize=1)
        results = [None] * len(cases)
        for i, prog in zip(todo, found):
            results[i] = prog
        old = sum(len(base[i]) for i in todo)
        new = sum(len(results[i]) if results[i] else len(base[i]) for i in todo)
        improved = sum(1 for r in results if r)
        print(f"{name}: {old} -> {new} (poprawione {improved}/{len(todo)})", flush=True)
        write_programs(CANDIDATE_DIR / f"beam{args.tag}" / output_name(name), results)


if __name__ == "__main__":
    main()
