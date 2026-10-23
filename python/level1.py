"""Rozwiazanie Level 1: tekstowa reprezentacja szesciokatnej planszy."""

import argparse
from pathlib import Path


def render_board(radius: int) -> str:
    if radius < 0:
        raise ValueError("Promien nie moze byc ujemny.")

    rows = []
    for y in range(-radius, radius + 1):
        padding = "-" * abs(y)
        cells = 2 * radius + 1 - abs(y)

        if abs(y) == radius:
            symbols = "*" * cells
        else:
            symbols = "*" + "O" * (cells - 2) + "*"

        rows.append(padding + "-".join(symbols) + padding)

    return "\n".join(rows)


def solve(data: str) -> str:
    values = list(map(int, data.split()))
    if not values or values[0] != len(values) - 1:
        raise ValueError("Pierwsza liczba musi okreslac liczbe promieni.")

    boards = [render_board(radius) for radius in values[1:]]
    return "\n\n".join(boards) + ("\n" if boards else "")


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path, help="Pliki wejsciowe")
    parser.add_argument("--output-dir", type=Path, default=script_dir / "outputs")
    args = parser.parse_args()

    inputs = args.inputs or sorted(
        (script_dir.parent / "level-1").glob("in_level-1_*.txt")
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
