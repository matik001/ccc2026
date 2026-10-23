"""Rozwiazanie Level 4: program sterujacy szescioma emiterami."""

import argparse
from collections import Counter
from pathlib import Path

from level1 import render_board
from level2 import DIRECTIONS


def parse_programs(data: str) -> list[tuple[int, list[tuple[str, ...]]]]:
    lines = [line.strip() for line in data.splitlines() if line.strip()]
    if not lines:
        raise ValueError("Brak danych wejsciowych.")
    count = int(lines[0])
    if count < 0:
        raise ValueError("Liczba programow nie moze byc ujemna.")

    programs = []
    index = 1
    for _ in range(count):
        if index + 1 >= len(lines):
            raise ValueError("Brakuje promienia lub dlugosci programu.")
        radius, length = int(lines[index]), int(lines[index + 1])
        if radius < 0 or length < 0:
            raise ValueError("Promien i dlugosc programu nie moga byc ujemne.")
        index += 2
        program = [tuple(line.split()) for line in lines[index:index + length]]
        if len(program) != length:
            raise ValueError("Brakuje instrukcji programu.")
        if any(len(tick) != 6 or any(op not in {"F", "B", "T", "-"} for op in tick)
               for tick in program):
            raise ValueError("Wiersz musi zawierac 6 instrukcji F, B, T lub -.")
        programs.append((radius, program))
        index += length

    if index != len(lines):
        raise ValueError("Nadmiarowe dane po ostatnim programie.")
    return programs


def border_positions(radius: int) -> list[tuple[int, int]]:
    """Obwod zgodnie z ruchem wskazowek zegara, od lewego naroznika."""
    if radius < 0:
        raise ValueError("Promien nie moze byc ujemny.")
    if radius == 0:
        return [(0, 0)]

    border = []
    row, col = radius, 0
    for delta_row, delta_col in [(-1, 1), (0, 2), (1, 1), (1, -1), (0, -2), (-1, -1)]:
        for _ in range(radius):
            border.append((row, col))
            row += delta_row
            col += delta_col
    return border


def simulate(radius: int, program: list[tuple[str, ...]]) -> list[str]:
    board = [list(row) for row in render_board(radius).splitlines()]
    height, width = len(board), len(board[0])
    border = border_positions(radius)
    positions = [emitter * radius for emitter in range(6)]
    active = [False] * 6

    for instructions in program:
        # Najpierw wykonujemy wszystkie instrukcje danego kroku.
        for emitter, instruction in enumerate(instructions):
            if instruction == "F":
                positions[emitter] = (positions[emitter] + 1) % len(border)
            elif instruction == "B":
                positions[emitter] = (positions[emitter] - 1) % len(border)
            elif instruction == "T":
                active[emitter] = not active[emitter]

        # Emitery sa niezalezne, nawet jesli zajmuja to samo pole obwodu.
        hits = Counter()
        for emitter in range(6):
            if not active[emitter]:
                continue
            row, col = border[positions[emitter]]
            delta_row, delta_col = DIRECTIONS[str(emitter + 1)]
            row += delta_row
            col += delta_col
            while 0 <= row < height and 0 <= col < width:
                if board[row][col] in ("O", "M"):
                    hits[row, col] += 1
                row += delta_row
                col += delta_col

        # Stan materii zachowujemy pomiedzy kolejnymi krokami programu.
        for (row, col), count in hits.items():
            if count == 2:
                board[row][col] = "M"
            elif count >= 3:
                board[row][col] = "O"

    # W zadaniu dozwolony jest wydruk obwodu bez emiterow.
    return ["".join(row) for row in board]


def solve(data: str) -> str:
    boards = ["\n".join(simulate(radius, program))
              for radius, program in parse_programs(data)]
    return "\n\n".join(boards) + ("\n" if boards else "")


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path, help="Pliki wejsciowe")
    parser.add_argument("--output-dir", type=Path, default=script_dir / "outputs")
    args = parser.parse_args()

    inputs = args.inputs or sorted(
        (script_dir.parent / "level-4").glob("in_level-4_*.txt")
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
