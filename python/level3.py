"""Rozwiazanie Level 3: jednoczesne dzialanie wielu emiterow."""

import argparse
from pathlib import Path

from level2 import DIRECTIONS, parse_boards


def simulate(board: list[str]) -> list[str]:
    height, width = len(board), len(board[0])
    hits = [[0] * width for _ in range(height)]

    # Najpierw zliczamy wszystkie wiazki, nie zmieniajac stanu planszy.
    for emitter_row, line in enumerate(board):
        for emitter_col, symbol in enumerate(line):
            if symbol not in DIRECTIONS:
                continue

            delta_row, delta_col = DIRECTIONS[symbol]
            row = emitter_row + delta_row
            col = emitter_col + delta_col
            while 0 <= row < height and 0 <= col < width:
                if board[row][col] in ("O", "M"):
                    hits[row][col] += 1
                row += delta_row
                col += delta_col

    # Dokladnie 2 wiazki tworza materie, co najmniej 3 ja niszcza.
    # Przy 0 lub 1 wiazce zachowujemy poprzedni stan komorki.
    result = [list(line) for line in board]
    for row in range(height):
        for col in range(width):
            if board[row][col] not in ("O", "M"):
                continue
            if hits[row][col] == 2:
                result[row][col] = "M"
            elif hits[row][col] >= 3:
                result[row][col] = "O"

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
        (script_dir.parent / "level-3").glob("in_level-3_*.txt")
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
