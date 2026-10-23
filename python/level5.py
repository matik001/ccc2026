"""Rozwiazanie Level 5: zliczanie wyprodukowanych i odebranych wzorow."""

import argparse
from collections import Counter
from pathlib import Path

from level2 import DIRECTIONS
from level4 import border_positions


def parse_programs(data: str) -> list[tuple[int, list[str], list[tuple[str, ...]]]]:
    lines = [line.strip() for line in data.splitlines() if line.strip()]
    if not lines:
        raise ValueError("Brak danych wejsciowych.")
    count = int(lines[0])
    if count < 0:
        raise ValueError("Liczba programow nie moze byc ujemna.")

    programs = []
    index = 1
    for _ in range(count):
        if index >= len(lines):
            raise ValueError("Brakuje wymiarow wzoru.")
        radius, height, width = map(int, lines[index].split())
        if radius < 0 or height != 2 * radius + 1 or width != 4 * radius + 1:
            raise ValueError("Niepoprawne wymiary wzoru.")
        index += 1
        pattern = lines[index:index + height]
        if len(pattern) != height or any(len(row) != width for row in pattern):
            raise ValueError("Wzor nie odpowiada podanym wymiarom.")
        index += height

        if index >= len(lines):
            raise ValueError("Brakuje dlugosci programu.")
        length = int(lines[index])
        if length < 0:
            raise ValueError("Dlugosc programu nie moze byc ujemna.")
        index += 1
        program = [tuple(line.split()) for line in lines[index:index + length]]
        if len(program) != length:
            raise ValueError("Brakuje instrukcji programu.")
        if any(len(tick) != 6 or any(op not in {"F", "B", "T", "-"} for op in tick)
               for tick in program):
            raise ValueError("Wiersz musi zawierac 6 instrukcji F, B, T lub -.")
        programs.append((radius, pattern, program))
        index += length

    if index != len(lines):
        raise ValueError("Nadmiarowe dane po ostatnim programie.")
    return programs


def simulate(radius: int, pattern_board: list[str], program: list[tuple[str, ...]]) -> int:
    height, width = len(pattern_board), len(pattern_board[0])
    cells = {(row, col)
             for row, line in enumerate(pattern_board)
             for col, symbol in enumerate(line) if symbol in ("O", "M")}
    pattern = {(row, col)
               for row, line in enumerate(pattern_board)
               for col, symbol in enumerate(line) if symbol == "M"}
    if not pattern:
        raise ValueError("Wzor musi zawierac przynajmniej jedna komorke materii.")

    # Szesciu sasiadow kazdej komorki wzoru, z wylaczeniem samego wzoru.
    neighbours = {(row + dr, col + dc)
                  for row, col in pattern for dr, dc in DIRECTIONS.values()}
    neighbours = (neighbours & cells) - pattern

    border = border_positions(radius)
    positions = [emitter * radius for emitter in range(6)]
    active = [False] * 6
    matter = set()  # Wzor opisuje cel; rzeczywista plansza zaczyna pusta.
    submitted = 0

    for instructions in program:
        # 1. Jednoczesnie wykonujemy instrukcje wszystkich emiterow.
        for emitter, instruction in enumerate(instructions):
            if instruction == "F":
                positions[emitter] = (positions[emitter] + 1) % len(border)
            elif instruction == "B":
                positions[emitter] = (positions[emitter] - 1) % len(border)
            elif instruction == "T":
                active[emitter] = not active[emitter]

        # 2. Zliczamy wiazki, a nastepnie tworzymy i niszczymy materie.
        hits = Counter()
        for emitter in range(6):
            if not active[emitter]:
                continue
            row, col = border[positions[emitter]]
            dr, dc = DIRECTIONS[str(emitter + 1)]
            row += dr
            col += dc
            while 0 <= row < height and 0 <= col < width:
                if (row, col) in cells:
                    hits[row, col] += 1
                row += dr
                col += dc

        for cell, count in hits.items():
            if count == 2:
                matter.add(cell)
            elif count >= 3:
                matter.discard(cell)

        # 3. Odbieramy wzor tylko w dokladnej pozycji i bez sasiadujacej materii.
        # Odlegla materia pozostaje na planszy na kolejne kroki programu.
        if pattern <= matter and neighbours.isdisjoint(matter):
            submitted += 1
            matter.difference_update(pattern)

    return submitted


def solve(data: str) -> str:
    results = [str(simulate(radius, pattern, program))
               for radius, pattern, program in parse_programs(data)]
    return "\n".join(results) + ("\n" if results else "")


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path, help="Pliki wejsciowe")
    parser.add_argument("--output-dir", type=Path, default=script_dir / "outputs")
    args = parser.parse_args()

    inputs = args.inputs or sorted(
        (script_dir.parent / "level-5").glob("in_level-5_*.txt")
    )
    if not inputs:
        parser.error("Nie znaleziono plikow wejsciowych.")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    for input_path in inputs:
        result = solve(input_path.read_text(encoding="utf-8-sig"))
        output_name = "out_" + input_path.name.removeprefix("in_")
        output_path = args.output_dir / output_name
        output_path.write_text(result, encoding="utf-8", newline="\n")
        print(f"{input_path.name} -> {output_path}")


if __name__ == "__main__":
    main()
