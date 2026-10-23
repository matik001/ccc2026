"""Rekombinacja segmentow miedzy odbiorami z wielu programow.

Po kazdym odbiorze P jest puste, a H czyste, wiec dalszy przebieg zalezy tylko od
konfiguracji emiterow (pozycje + aktywnosc). Fragment programu miedzy kolejnymi
odbiorami to krawedz konfiguracja -> konfiguracja o znanej dlugosci. Zbieramy
krawedzie ze wszystkich kandydatow i szukamy najkrotszej sciezki z dokladnie req
krawedziami (DP po liczbie odbiorow). Stany rozniace sie zamiana emiterow a <-> a+3
sa utozsamiane (z przemapowaniem ich opow).
"""

import argparse
from multiprocessing import Pool
from pathlib import Path

from l6_core import CANDIDATE_DIR, NAMES, OUTPUT_DIR, load_cases, output_name, read_programs, write_programs

SOURCES = [OUTPUT_DIR, *sorted(p for p in CANDIDATE_DIR.iterdir() if p.is_dir())]
CPP = Path(__file__).resolve().parent.parent / "cpp"
for sub in ("outputs", "candidates", "imported_candidates"):
    d = CPP / sub
    if d.is_dir():
        SOURCES += [d] if sub == "outputs" else sorted(p for p in d.iterdir() if p.is_dir())


class Frame:
    """Wspolrzedne linii i kanonizacja stanu wzgledem zamian w parach."""

    def __init__(self, case):
        self.case = case
        self.sgn = []
        L = case.L
        for a in range(3):
            q = (case.home(a + 3) + 1) % L
            sgn = 1
            for s in (1, -1):
                p = (case.home(a) + s) % L
                if case.ray[a][p] and case.ray[a + 3][q] and case.ray[a][p] == case.ray[a + 3][q]:
                    sgn = s
            self.sgn.append(sgn)

    def coord(self, e, p):
        d = self.case.delta(self.case.home(e), p)
        return d if e < 3 else self.sgn[e - 3] * d

    def canon(self, pos, act):
        """(klucz kanoniczny, zbior osi zamienionych wzgledem stanu fizycznego)."""
        key = []
        swapped = []
        for a in range(3):
            u = (self.coord(a, pos[a]), act >> a & 1)
            v = (self.coord(a + 3, pos[a + 3]), act >> (a + 3) & 1)
            if u > v:
                u, v = v, u
                swapped.append(a)
            key.append((u, v))
        return tuple(key), tuple(swapped)

    def relabel(self, ticks, axes):
        """Zamienia role emiterow a <-> a+3 dla podanych osi."""
        if not axes:
            return list(ticks)
        out = []
        for tick in ticks:
            t = list(tick)
            for a in axes:
                fix = (lambda op: op) if self.sgn[a] == 1 else (lambda op: {"F": "B", "B": "F"}.get(op, op))
                t[a], t[a + 3] = fix(tick[a + 3]), fix(tick[a])
            out.append("".join(t))
        return out


def segments(case, frame, prog):
    """Krawedzie (klucz_start, klucz_koniec, opy w ramce kanonicznej startu)."""
    pos = [case.home(e) for e in range(6)]
    act = 0
    matter = 0
    start_t = 0
    start_key, start_sw = frame.canon(pos, act)
    edges = []
    for t, tick in enumerate(prog):
        for e in range(6):
            op = tick[e]
            if op == "F":
                pos[e] = (pos[e] + 1) % case.L
            elif op == "B":
                pos[e] = (pos[e] - 1) % case.L
            elif op == "T":
                act ^= 1 << e
        if act:
            c2, c3 = case.config(tuple(pos), act)
            matter = (matter | c2) & ~c3
        if matter & case.P == case.P and not matter & case.H:
            matter &= ~case.P
            end_key, end_sw = frame.canon(pos, act)
            ops = frame.relabel(prog[start_t:t + 1], start_sw)
            edges.append((start_key, end_key, ops))
            start_t, start_key, start_sw = t + 1, end_key, end_sw
    return edges


def solve(args):
    case, programs = args
    frame = Frame(case)
    best_edge = {}
    for prog in programs:
        if not prog:
            continue
        for a, b, ops in segments(case, frame, prog):
            old = best_edge.get((a, b))
            if old is None or len(ops) < len(old):
                best_edge[(a, b)] = ops
    start, _ = frame.canon([case.home(e) for e in range(6)], 0)
    out = {}
    for (a, b), ops in best_edge.items():
        out.setdefault(a, []).append((b, ops))
    # dp[node] = (dlugosc, poprzednik) dla k odbiorow
    layer = {start: (0, None)}
    history = []
    for _ in range(case.req):
        nxt = {}
        for node, (dist, _) in layer.items():
            for b, ops in out.get(node, []):
                nd = dist + len(ops)
                if b not in nxt or nd < nxt[b][0]:
                    nxt[b] = (nd, (node, ops))
        history.append(nxt)
        layer = nxt
        if not layer:
            return None
    end = min(layer, key=lambda k: layer[k][0])
    chain = []
    node = end
    for k in range(case.req - 1, -1, -1):
        _, (prev, ops) = history[k][node]
        chain.append(ops)
        node = prev
    chain.reverse()
    # Skladanie: opy kazdego segmentu sa w ramce kanonicznej jego startu, wiec
    # przemapowujemy je na biezacy stan fizyczny.
    prog = []
    pos = [case.home(e) for e in range(6)]
    act = 0
    for ops in chain:
        _, sw = frame.canon(pos, act)
        phys = frame.relabel(ops, sw)
        for tick in phys:
            for e in range(6):
                op = tick[e]
                if op == "F":
                    pos[e] = (pos[e] + 1) % case.L
                elif op == "B":
                    pos[e] = (pos[e] - 1) % case.L
                elif op == "T":
                    act ^= 1 << e
        prog.extend(phys)
    return case.finish(prog)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", default=NAMES)
    parser.add_argument("--jobs", type=int, default=8)
    args = parser.parse_args()
    for name in args.names:
        cases = load_cases(name)
        per_source = [read_programs(d / output_name(name), len(cases)) for d in SOURCES]
        base = per_source[0]
        tasks = [(case, [src[i] for src in per_source]) for i, case in enumerate(cases)]
        with Pool(args.jobs) as pool:
            found = pool.map(solve, tasks, chunksize=4)
        results = [f if f and base[i] and len(f) < len(base[i]) else None for i, f in enumerate(found)]
        old = sum(len(p) for p in base)
        new = sum(len(r) if r else len(p) for r, p in zip(results, base))
        print(f"{name}: {old} -> {new} (poprawione {sum(1 for r in results if r)}/{len(cases)}, zrodel {len(SOURCES)})", flush=True)
        write_programs(CANDIDATE_DIR / "splice" / output_name(name), results)


if __name__ == "__main__":
    main()
