"""Level 6: generator krotkich programow produkujacych zadane wzory.

Portfolio porownuje trasy dwoch i trzech wiazek, przycinane pelne linie
oraz wypelnianie gestych wzorow z pozniejszym usuwaniem nadmiaru.
Nie gwarantuje globalnie najkrotszego programu.
"""

import argparse
from collections import Counter, deque
from itertools import product
from pathlib import Path
from random import Random

from level2 import DIRECTIONS
from level4 import border_positions
from level5 import simulate as count_submissions


def parse_patterns(data: str) -> list[tuple[int, list[str], int]]:
    lines = [line.strip() for line in data.splitlines() if line.strip()]
    if not lines:
        raise ValueError("Brak danych wejsciowych.")
    count = int(lines[0])
    if count < 0:
        raise ValueError("Liczba wzorow nie moze byc ujemna.")
    cases = []
    index = 1
    for _ in range(count):
        if index >= len(lines):
            raise ValueError("Brakuje naglowka wzoru.")
        radius, height, width, required = map(int, lines[index].split())
        if radius < 1 or height != 2 * radius + 1 or width != 4 * radius + 1 or required < 1:
            raise ValueError("Niepoprawne wymiary lub liczba wymaganych wzorow.")
        index += 1
        board = lines[index:index + height]
        if len(board) != height or any(len(row) != width for row in board):
            raise ValueError("Wzor nie odpowiada podanym wymiarom.")
        if not any("M" in row for row in board):
            raise ValueError("Wzor nie moze byc pusty.")
        cases.append((radius, board, required))
        index += height
    if index != len(lines):
        raise ValueError("Nadmiarowe dane po ostatnim wzorze.")
    return cases


def cyclic_delta(start: int, end: int, perimeter: int) -> int:
    delta = (end - start) % perimeter
    return delta if delta <= perimeter // 2 else delta - perimeter


def move_command(delta: int) -> str:
    return "F" if delta > 0 else "B" if delta < 0 else "-"


def source_offsets(radius: int, cells: list[tuple[int, int]]) -> list[tuple[int, ...]]:
    """Pozycje emiterow oswietlajacych komorke, wzgledem ich naroznikow."""
    rail = {point: index for index, point in enumerate(border_positions(radius))}
    offsets = []
    for row, col in cells:
        sources = []
        for emitter, (dr, dc) in enumerate(DIRECTIONS.values()):
            r, c = row, col
            while (r, c) not in rail:
                r -= dr
                c -= dc
                if not (0 <= r <= 2 * radius and 0 <= c <= 4 * radius):
                    raise ValueError("Komorka wzoru lezy poza wnetrzem planszy.")
            sources.append(cyclic_delta(emitter * radius, rail[r, c], 6 * radius))
        offsets.append(tuple(sources))
    return offsets


def initial_moves(emitters: tuple[int, ...], offsets: tuple[int, ...]) -> list[tuple[str, ...]]:
    """Ustawia nieaktywne emitery, poruszajac nimi jednoczesnie."""
    program = []
    for step in range(max(map(abs, offsets), default=0)):
        tick = ["-"] * 6
        for emitter, delta in zip(emitters, offsets):
            if step < abs(delta):
                tick[emitter] = move_command(delta)
        program.append(tuple(tick))
    tick = ["-"] * 6
    for emitter in emitters:
        tick[emitter] = "T"
    program.append(tuple(tick))
    return program


def emit_walk(emitters: tuple[int, int], states: list[tuple[int, int]],
              required: int) -> list[tuple[str, ...]]:
    program = initial_moves(emitters, states[0])
    path = states
    for repetition in range(required):
        if repetition:
            # Po odbiorze wzoru malujemy ponownie biezacy punkt i zawracamy.
            program.append(tuple("------"))
            path = list(reversed(path))
        for before, after in zip(path, path[1:]):
            tick = ["-"] * 6
            for emitter, old, new in zip(emitters, before, after):
                assert abs(new - old) <= 1
                tick[emitter] = move_command(new - old)
            program.append(tuple(tick))
    return program


def cover_walk(graph: list[list[int]], start: int, rng: Random) -> list[int]:
    """Zachlanna trasa przez wszystkie pola; BFS laczy pozostale fragmenty."""
    unseen = set(range(len(graph)))
    degree = [len(neighbours) for neighbours in graph]
    path = [start]
    unseen.remove(start)
    for neighbour in graph[start]:
        degree[neighbour] -= 1

    while unseen:
        current = path[-1]
        options = [node for node in graph[current] if node in unseen]
        if options:
            # Najpierw pola z mala liczba dostepnych wyjsc, aby ograniczyc powroty.
            destination = min(options, key=lambda node: (degree[node], rng.random()))
            extension = [destination]
        else:
            queue = deque([current])
            parents = {current: None}
            destination = None
            while queue and destination is None:
                node = queue.popleft()
                neighbours = list(graph[node])
                rng.shuffle(neighbours)
                for neighbour in neighbours:
                    if neighbour in parents:
                        continue
                    parents[neighbour] = node
                    if neighbour in unseen:
                        destination = neighbour
                        break
                    queue.append(neighbour)
            if destination is None:
                raise ValueError("Wzor musi byc spojny.")
            extension = [destination]
            while parents[extension[-1]] != current:
                extension.append(parents[extension[-1]])
            extension.reverse()
        path.extend(extension)
        unseen.remove(destination)
        for neighbour in graph[destination]:
            degree[neighbour] -= 1

    # Usuwamy zbedne powtorzenia bez pomijania zadnego pola.
    counts = Counter(path)
    while len(path) > 1 and counts[path[0]] > 1:
        counts[path.pop(0)] -= 1
    index = 1
    while index + 1 < len(path):
        node = path[index]
        if counts[node] > 1 and path[index + 1] in graph[path[index - 1]]:
            counts[node] -= 1
            path.pop(index)
            index = max(1, index - 1)
        else:
            index += 1
    return path


def generate_point_program(radius: int, board: list[str], required: int,
                           attempts: int = 32) -> list[tuple[str, ...]]:
    cells = [(r, c) for r, row in enumerate(board)
             for c, value in enumerate(row) if value in ("O", "M")]
    target = [(r, c) for r, c in cells if board[r][c] == "M"]
    sources = source_offsets(radius, cells)
    by_cell = dict(zip(cells, sources))
    target_sources = [by_cell[cell] for cell in target]
    best = None
    best_score = float("inf")
    rng = Random(6000 + radius)

    for emitters in ((0, 1), (0, 2), (1, 2)):
        coordinates = [(source[emitters[0]], source[emitters[1]]) for source in target_sources]
        lookup = {point: i for i, point in enumerate(coordinates)}
        graph = [[lookup[x + dx, y + dy]
                  for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                  if (dx or dy) and (x + dx, y + dy) in lookup]
                 for x, y in coordinates]
        setup = [max(abs(x), abs(y)) for x, y in coordinates]
        starts = sorted(range(len(target)), key=lambda i: (setup[i], len(graph[i])))
        lower_bound = required * len(target) + min(setup)
        for attempt in range(attempts):
            start = starts[attempt % len(starts)] if attempt < 8 else rng.randrange(len(target))
            path = cover_walk(graph, start, rng)
            if setup[path[-1]] < setup[path[0]]:
                path.reverse()
            score = setup[path[0]] + required * len(path)
            if score < best_score:
                best_score = score
                best = emit_walk(emitters, [coordinates[i] for i in path], required)
            if score == lower_bound:
                break

    # Przeciwlegle emitery moga wypelnic caly rzad w jednym kroku.
    target_set = set(target)
    for axis in range(3):
        groups = {}
        for cell, source in zip(cells, sources):
            groups.setdefault(source[axis], []).append(cell)
        selected = sorted({by_cell[cell][axis] for cell in target})
        if any(not set(groups[value]) <= target_set for value in selected):
            continue
        if any(b - a != 1 for a, b in zip(selected, selected[1:])):
            continue
        emitters = (axis, axis + 3)
        states = [(value, by_cell[groups[value][0]][axis + 3]) for value in selected]
        if max(map(abs, states[-1])) < max(map(abs, states[0])):
            states.reverse()
        program = emit_walk(emitters, states, required)
        if len(program) < best_score:
            best, best_score = program, len(program)

    # Trzy nieparalelne wiazki tworza do trzech pol w jednym kroku.
    if len(target) <= 3:
        ray_sets = [{value: {cell for cell, source in zip(cells, sources) if source[axis] == value}
                     for value in {source[axis] for source in target_sources}}
                    for axis in range(3)]
        for offsets in product(*(rays.keys() for rays in ray_sets)):
            a, b, c = [ray_sets[axis][offsets[axis]] for axis in range(3)]
            created = ((a & b) - c) | ((a & c) - b) | ((b & c) - a)
            if created != target_set:
                continue
            program = initial_moves((0, 1, 2), offsets)
            program.extend([tuple("------")] * (required - 1))
            if len(program) < best_score:
                best, best_score = program, len(program)

    assert best is not None and len(best) == best_score
    return best


def generate_program(radius: int, board: list[str], required: int,
                     attempts: int = 64,
                     incumbent: list[tuple[str, ...]] | None = None) -> list[tuple[str, ...]]:
    from level6_dense import generate_candidate as dense_candidate
    from level6_multibeam import generate_candidate as multibeam_candidate
    from level6_routes import generate_candidate as route_candidate
    from level6_static import generate_candidate as raster_candidate

    best = generate_point_program(radius, board, required, min(attempts, 32))

    def consider(candidate):
        nonlocal best
        if candidate is not None and len(candidate) < len(best):
            # Odbior moze nastapic juz podczas przejscia miedzy konfiguracjami.
            # Sprawdzamy rzeczywista liczbe odbiorow, a nie tylko pokrycie pol.
            if count_submissions(radius, board, candidate) == required:
                best = candidate

    consider(incumbent)
    consider(raster_candidate(radius, board, required, limit=len(best)))
    consider(dense_candidate(radius, board, required, budget=len(best)))
    consider(multibeam_candidate(radius, board, required, attempts=attempts,
                                rotations=1000, budget=len(best)))
    consider(route_candidate(radius, board, required, incumbent=best))
    return best


def solve(data: str, attempts: int = 64, previous: str | None = None) -> str:
    programs = []
    cases = parse_patterns(data)
    previous_programs = []
    if previous and previous.strip():
        blocks = previous.strip().split("\n\n")
        if len(blocks) == len(cases):
            for block in blocks:
                program = [tuple(line.split()) for line in block.splitlines()]
                valid = all(len(tick) == 6 and all(op in {"F", "B", "T", "-"} for op in tick)
                            for tick in program)
                previous_programs.append(program if valid else None)
    for index, (radius, board, required) in enumerate(cases, 1):
        incumbent = previous_programs[index - 1] if previous_programs else None
        program = generate_program(radius, board, required, attempts, incumbent)
        actual = count_submissions(radius, board, program)
        if actual != required:
            raise AssertionError(f"Wzor {index}: oczekiwano {required}, uzyskano {actual}.")
        programs.append("\n".join(" ".join(tick) for tick in program))
    return "\n\n".join(programs) + ("\n" if programs else "")


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("inputs", nargs="*", type=Path, help="Pliki wejsciowe")
    parser.add_argument("--output-dir", type=Path, default=script_dir / "outputs")
    parser.add_argument("--attempts", type=int, default=64, help="Liczba prob trasy dla trzech wiazek")
    args = parser.parse_args()
    if args.attempts < 1:
        parser.error("Liczba prob musi byc dodatnia.")
    inputs = args.inputs or sorted((script_dir.parent / "level-6").glob("in_level-6_*.txt"))
    if not inputs:
        parser.error("Nie znaleziono plikow wejsciowych.")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    for input_path in inputs:
        output_path = args.output_dir / ("out_" + input_path.name.removeprefix("in_"))
        previous = output_path.read_text(encoding="utf-8") if output_path.exists() else None
        result = solve(input_path.read_text(encoding="utf-8-sig"), args.attempts, previous)
        output_path.write_text(result, encoding="utf-8", newline="\n")
        score = sum(bool(line) for line in result.splitlines())
        print(f"{input_path.name} -> {output_path} ({score} instrukcji)", flush=True)


if __name__ == "__main__":
    main()
