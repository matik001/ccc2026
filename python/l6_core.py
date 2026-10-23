"""Wspolny rdzen level 6: geometria, maski bitowe i szybki symulator.

Plansza w ukladzie wzoru (radius n, wysokosc 2n+1, szerokosc 4n+1), obwod
z level4.border_positions, kierunki z level2.DIRECTIONS - tak jak level5.simulate.
Komorki wnetrza to bity jednej liczby int.
"""

from pathlib import Path

from level2 import DIRECTIONS
from level4 import border_positions
from level6 import parse_patterns

ROOT = Path(__file__).resolve().parent
INPUT_DIR = ROOT.parent / "level-6"
OUTPUT_DIR = ROOT / "outputs"
CANDIDATE_DIR = ROOT / "candidates"
NAMES = ["0-example", "1-small", "2-medium", "3-large", "4-enormous"]
IDLE = "------"


def input_path(name: str) -> Path:
    return INPUT_DIR / f"in_level-6_{name}.txt"


def output_name(name: str) -> str:
    return f"out_level-6_{name}.txt"


def counts(masks):
    """Bit-slice: (dokladnie 2 trafienia, co najmniej 3 trafienia)."""
    one = two = three = 0
    for m in masks:
        three |= two & m
        two = (two & ~m) | (one & m)
        one = (one & ~m) | (m & ~(one | two | three))
    return two, three


class Case:
    def __init__(self, radius: int, board: list[str], required: int):
        self.n = radius
        self.board = board
        self.req = required
        self.L = 6 * radius
        self.cells = [(r, c) for r, row in enumerate(board)
                      for c, ch in enumerate(row) if ch in "OM"]
        self.index = {cell: i for i, cell in enumerate(self.cells)}
        self.P = 0
        self.H = 0
        for (r, c), i in self.index.items():
            if board[r][c] == "M":
                self.P |= 1 << i
        for (r, c), i in self.index.items():
            if board[r][c] == "O" and any(
                    (r + dr, c + dc) in self.index and board[r + dr][c + dc] == "M"
                    for dr, dc in DIRECTIONS.values()):
                self.H |= 1 << i
        self.p_count = self.P.bit_count()
        self.border = border_positions(radius)
        height, width = len(board), len(board[0])
        # ray[e][p]: maska komorek trafianych przez emiter e stojacy na pozycji p
        self.ray = []
        for e, (dr, dc) in enumerate(DIRECTIONS.values()):
            masks = []
            for r0, c0 in self.border:
                m = 0
                r, c = r0 + dr, c0 + dc
                while 0 <= r < height and 0 <= c < width:
                    i = self.index.get((r, c))
                    if i is not None:
                        m |= 1 << i
                    r += dr
                    c += dc
                masks.append(m)
            self.ray.append(masks)
        # Pozycje, z ktorych emiter w ogole trafia we wnetrze (offset wzgledem startu).
        self.useful = [[p for p in range(self.L) if self.ray[e][p]] for e in range(6)]
        self._cfg = {}

    def home(self, e: int) -> int:
        return e * self.n

    def delta(self, a: int, b: int) -> int:
        d = (b - a) % self.L
        return d if d <= self.L // 2 else d - self.L

    def config(self, pos, active):
        """(C2, C3) dla stanu emiterow; active to maska bitowa 6 emiterow."""
        key = (pos, active)
        res = self._cfg.get(key)
        if res is None:
            res = counts([self.ray[e][pos[e]] for e in range(6) if active >> e & 1])
            if len(self._cfg) < 200000:
                self._cfg[key] = res
        return res

    def run(self, program, stop=True):
        """Zwraca (liczba odbiorow, dlugosc programu do req-tego odbioru lub None)."""
        pos = [self.home(e) for e in range(6)]
        active = 0
        matter = 0
        acc = 0
        P, H, L = self.P, self.H, self.L
        for t, tick in enumerate(program):
            for e in range(6):
                op = tick[e]
                if op == "F":
                    pos[e] = (pos[e] + 1) % L
                elif op == "B":
                    pos[e] = (pos[e] - 1) % L
                elif op == "T":
                    active ^= 1 << e
            if active:
                c2, c3 = self.config(tuple(pos), active)
                matter = (matter | c2) & ~c3
            if matter & P == P and not matter & H:
                acc += 1
                matter &= ~P
                if acc == self.req and stop:
                    return acc, t + 1
        return acc, (len(program) if acc == self.req else None)

    def finish(self, program):
        """Program obciety do req-tego odbioru albo None, gdy niepoprawny."""
        acc, length = self.run(program)
        if acc != self.req or length is None:
            return None
        return list(program[:length])


def load_cases(name: str) -> list[Case]:
    text = input_path(name).read_text(encoding="utf-8-sig")
    return [Case(r, b, q) for r, b, q in parse_patterns(text)]


def read_programs(path: Path, count: int):
    """Lista programow (lista 6-znakowych napisow) albo None dla brakujacych."""
    if not path.exists():
        return [None] * count
    blocks = path.read_text(encoding="utf-8").strip().split("\n\n")
    result = []
    for block in blocks:
        lines = [line.split() for line in block.strip().splitlines() if line.strip()]
        if lines and all(len(x) == 6 and all(op in "FBT-" for op in x) for x in lines):
            result.append(["".join(x) for x in lines])
        else:
            result.append(None)
    if len(result) != count:
        return [None] * count
    return result


def write_programs(path: Path, programs) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    blocks = []
    for program in programs:
        # Brak programu zapisujemy jako znacznik, zeby nie gubic podzialu na bloki.
        blocks.append("\n".join(" ".join(tick) for tick in program) if program else "NONE")
    path.write_text("\n\n".join(blocks) + "\n", encoding="utf-8", newline="\n")


def baseline(name: str, cases: list[Case]):
    return read_programs(OUTPUT_DIR / output_name(name), len(cases))


if __name__ == "__main__":
    # Kontrola zgodnosci: obecne wyjscia musza dawac dokladnie req odbiorow.
    from level5 import simulate
    total = 0
    for name in NAMES:
        cases = load_cases(name)
        progs = baseline(name, cases)
        score = 0
        for i, (case, prog) in enumerate(zip(cases, progs)):
            acc, length = case.run(prog, stop=False)
            ref = simulate(case.n, case.board, [tuple(t) for t in prog])
            assert acc == ref == case.req, (name, i, acc, ref, case.req)
            score += len(prog)
        total += score
        print(name, score)
    print("TOTAL", total)
