"""T1: cykle dla wielu powtorzen.

Emitery dojezdzaja do stanu S0, a potem powtarzaja cykl okresu k (kazdy emiter
wraca na swoje miejsce i do swojej aktywnosci). Wzor odtwarza sie co cykl,
wiec koszt to ~dojazd + k * req zamiast req pelnych budow.
Parametry cyklu dobiera wyzarzanie z restartami; ocena to dokladna symulacja.
"""

import argparse
import math
from itertools import product
from multiprocessing import Pool
from random import Random

from l6_core import CANDIDATE_DIR, IDLE, NAMES, baseline, load_cases, output_name, write_programs

MOVE = {"F": 1, "B": -1}


def sequences(k):
    """Sekwencje okresu k: zerowe przesuniecie i parzysta liczba przelaczen."""
    return ["".join(s) for s in product("FBT-", repeat=k)
            if s.count("F") == s.count("B") and s.count("T") % 2 == 0]


def random_sequence(k, rng):
    """Losowa poprawna sekwencja dla duzych k, gdzie pelna lista jest za duza."""
    ops = [rng.choice("FB-T-") for _ in range(k)]
    while ops.count("F") != ops.count("B"):
        extra = "F" if ops.count("F") > ops.count("B") else "B"
        i = rng.choice([i for i, op in enumerate(ops) if op == extra])
        ops[i] = rng.choice(["-", "B" if extra == "F" else "F"])
    if ops.count("T") % 2:
        ops[rng.choice([i for i, op in enumerate(ops) if op == "T"])] = "-"
    return "".join(ops)


class CycleModel:
    def __init__(self, case, k, cap):
        self.case = case
        self.k = k
        self.cap = cap
        self.seqs = sequences(k) if k <= 6 else None

    def random_seq(self, rng):
        return rng.choice(self.seqs) if self.seqs else random_sequence(self.k, rng)

    def states(self, s, a, q):
        """Konfiguracje S0..S(k-1) jako lista (pos, active)."""
        L = self.case.L
        pos = list(s)
        active = sum(1 << e for e in range(6) if a[e])
        result = [(tuple(pos), active)]
        for j in range(self.k - 1):
            for e in range(6):
                op = q[e][j]
                if op == "T":
                    active ^= 1 << e
                elif op != "-":
                    pos[e] = (pos[e] + MOVE[op]) % L
            result.append((tuple(pos), active))
        return result

    def prefix_length(self, s, a):
        case = self.case
        return max(abs(case.delta(case.home(e), s[e])) + a[e] for e in range(6))

    def evaluate(self, s, a, q):
        """Koszt: dlugosc programu, gdy osiaga req; inaczej kara powyzej cap."""
        case = self.case
        P, H, req = case.P, case.H, case.req
        configs = [case.config(pos, act) for pos, act in self.states(s, a, q)]
        k = self.k
        t = self.prefix_length(s, a)
        matter = 0
        acc = 0
        best_def = case.p_count
        j = 0
        if t == 0:
            # S0 nie strzela (wszystko wylaczone); pierwszy tick to S1.
            j = 1 % k
        else:
            t -= 1
        while t < self.cap:
            t += 1
            c2, c3 = configs[j]
            j += 1
            if j == k:
                j = 0
            matter = (matter | c2) & ~c3
            missing = P & ~matter
            bad = H & matter
            if not missing and not bad:
                acc += 1
                matter &= ~P
                best_def = case.p_count
                if acc == req:
                    return t
            else:
                d = missing.bit_count() + 2 * bad.bit_count()
                if d < best_def:
                    best_def = d
        return self.cap + 30 * (req - acc) + best_def

    def program(self, s, a, q):
        case = self.case
        T0 = self.prefix_length(s, a)
        prog = []
        for t in range(1, T0 + 1):
            tick = []
            for e in range(6):
                d = case.delta(case.home(e), s[e])
                start = T0 - abs(d) - a[e] + 1
                if start <= t < start + abs(d):
                    tick.append("F" if d > 0 else "B")
                elif a[e] and t == T0:
                    tick.append("T")
                else:
                    tick.append("-")
            prog.append("".join(tick))
        cycle = ["".join(q[e][j] for e in range(6)) for j in range(self.k)]
        # S0 jest stanem po prefiksie; kolejne ticki to ops cyklu (S1, ..., Sk=S0).
        need = self.cap + self.k
        while len(prog) < need:
            prog.extend(cycle)
        return case.finish(prog)


def anneal(model, rng, iterations):
    case = model.case
    idle = "-" * model.k
    span = case.n

    def random_pos(e):
        return (case.home(e) + rng.randint(-span + 1, span - 1)) % case.L

    s = [random_pos(e) for e in range(6)]
    a = [rng.random() < 0.7 for _ in range(6)]
    q = [model.random_seq(rng) if rng.random() < 0.5 else idle for _ in range(6)]
    cost = model.evaluate(s, a, q)
    best = (cost, list(s), list(a), list(q))
    temp0, temp1 = 4.0, 0.2
    for it in range(iterations):
        temp = temp0 * (temp1 / temp0) ** (it / iterations)
        e = rng.randrange(6)
        old = (s[e], a[e], q[e])
        r = rng.random()
        if r < 0.25:
            q[e] = model.random_seq(rng)
        elif r < 0.35:
            # Zamiana dwoch opow w sekwencji zachowuje jej poprawnosc.
            i, j = rng.randrange(model.k), rng.randrange(model.k)
            ops = list(q[e])
            ops[i], ops[j] = ops[j], ops[i]
            q[e] = "".join(ops)
        elif r < 0.6:
            s[e] = (s[e] + rng.choice((-1, 1))) % case.L
        elif r < 0.72:
            a[e] = not a[e]
        elif r < 0.82:
            s[e] = random_pos(e)
        elif r < 0.9:
            q[e] = idle
        else:
            # Zmiana fazy: ten sam orbit, inny punkt startowy cyklu.
            op = q[e][0]
            if op == "T":
                a[e] = not a[e]
            elif op != "-":
                s[e] = (s[e] + MOVE[op]) % case.L
            q[e] = q[e][1:] + q[e][0]
        new = model.evaluate(s, a, q)
        if new <= cost or rng.random() < math.exp((cost - new) / temp):
            cost = new
            if cost < best[0]:
                best = (cost, list(s), list(a), list(q))
        else:
            s[e], a[e], q[e] = old
    return best


def solve_case(args):
    case, base_len, ks, restarts, iterations, seed = args
    rng = Random(seed)
    best = None
    cap = base_len - 1
    for k in ks:
        for _ in range(restarts):
            model = CycleModel(case, k, cap)
            cost, s, a, q = anneal(model, rng, iterations)
            if cost <= cap:
                prog = model.program(s, a, q)
                if prog is not None and (best is None or len(prog) < len(best)):
                    best = prog
                    cap = len(prog) - 1
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("names", nargs="*", default=NAMES)
    parser.add_argument("--ks", default="1,2,3,4,5,6")
    parser.add_argument("--restarts", type=int, default=4)
    parser.add_argument("--iterations", type=int, default=4000)
    parser.add_argument("--min-req", type=int, default=2)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--jobs", type=int, default=16)
    parser.add_argument("--tag", default="", help="osobny katalog kandydatow dla tego przebiegu")
    args = parser.parse_args()
    ks = [int(x) for x in args.ks.split(",")]
    for name in args.names:
        cases = load_cases(name)
        base = baseline(name, cases)
        tasks = [(case, len(prog) if prog else 10 ** 6, ks, args.restarts, args.iterations,
                  args.seed * 100003 + i)
                 for i, (case, prog) in enumerate(zip(cases, base))]
        todo = [i for i, case in enumerate(cases) if case.req >= args.min_req]
        with Pool(args.jobs) as pool:
            found = pool.map(solve_case, [tasks[i] for i in todo], chunksize=1)
        results = [None] * len(cases)
        for i, prog in zip(todo, found):
            results[i] = prog
        old = sum(len(p) for p in base if p)
        new = sum(len(r) if r else len(p) for r, p in zip(results, base) if p)
        improved = sum(1 for r in results if r)
        print(f"{name}: {old} -> {new} (poprawione {improved}/{len(cases)})", flush=True)
        write_programs(CANDIDATE_DIR / f"cycles{args.tag}" / output_name(name), results)


if __name__ == "__main__":
    main()
