"""Rozwiazanie Level 2: symulacja lasera na szesciokatnej planszy."""

import argparse
from pathlib import Path


# Przesuniecia (wiersz, kolumna) w tekstowej reprezentacji planszy.
# W poziomie sasiednie komorki oddziela jeden znak '-'.
DIRECTIONS = {
    "1": (0, 2),    # prawo
    "2": (1, 1),    # dol-prawo
    "3": (1, -1),   # dol-lewo
    "4": (0, -2),   # lewo
    "5": (-1, -1),  # gora-lewo
    "6": (-1, 1),   # gora-prawo
}


def parse_boards(data: str) -> list[list[str]]:
    lines = [line.strip() for line in data.splitlines() if line.strip()]
    if not lines:
        raise ValueError("Brak danych wejsciowych.")

    count = int(lines[0])
    if count < 0:
        raise ValueError("Liczba plansz nie moze byc ujemna.")

    boards = []
    index = 1
    for _ in range(count):
        if index >= len(lines):
            raise ValueError("Brakuje wymiarow planszy.")
        radius, height, width = map(int, lines[index].split())
        if radius < 0 or height != 2 * radius + 1 or width != 4 * radius + 1:
            raise ValueError("Niepoprawne wymiary planszy.")
        index += 1
        board = lines[index:index + height]
        if len(board) != height or any(len(row) != width for row in board):
            raise ValueError("Plansza nie odpowiada podanym wymiarom.")
        boards.append(board)
        index += height

    if index != len(lines):
        raise ValueError("Nadmiarowe dane po ostatniej planszy.")
    return boards


def simulate(board: list[str]) -> list[str]:
    emitters = [
        (row, col, symbol)
        for row, line in enumerate(board)
        for col, symbol in enumerate(line)
        if symbol in DIRECTIONS
    ]
    if len(emitters) != 1:
        raise ValueError("Plansza musi zawierac dokladnie jeden emiter.")

    result = [list(line) for line in board]
    row, col, direction = emitters[0]
    delta_row, delta_col = DIRECTIONS[direction]
    row += delta_row
    col += delta_col

    while 0 <= row < len(result) and 0 <= col < len(result[0]):
        if result[row][col] == "O":
            result[row][col] = "M"
        row += delta_row
        col += delta_col

    return ["".join(line) for line in result]


def solve(data: str) -> str:
    boards = ["\n".join(simulate(board)) for board in parse_boards(data)]
    return "\n\n".join(boards) + ("\n" if boards else "")


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path, help="Pliki wejsciowe")
    parser.add_argument("--output-dir", type=Path, default=script_dir / "outputs")
    args = parser.parse_args()

    inputs = args.inputs or sorted(
        (script_dir.parent / "level-2").glob("in_level-2_*.txt")
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
