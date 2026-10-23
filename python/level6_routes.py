"""Two-beam route improvement by endpoint rotations and local shortcuts."""

from collections import Counter
from random import Random


def shorten_walk(path, adjacency):
    counts = Counter(path)
    while len(path) > 1 and counts[path[0]] > 1:
        counts[path.pop(0)] -= 1
    while len(path) > 1 and counts[path[-1]] > 1:
        counts[path.pop()] -= 1
    i = 1
    while i + 1 < len(path):
        vertex = path[i]
        if counts[vertex] > 1 and (path[i - 1] == path[i + 1]
                                  or path[i + 1] in adjacency[path[i - 1]]):
            counts[vertex] -= 1
            path.pop(i)
            i = max(1, i - 1)
        else:
            i += 1
    # A shortcut can remove several revisited cells at once even when no
    # individual cell can be removed safely.
    i = 0
    while i + 2 < len(path):
        available = Counter()
        best = None
        for j in range(i + 1, min(len(path), i + 12)):
            if j > i + 1 and (path[j] == path[i] or path[j] in adjacency[path[i]]):
                best = j
            available[path[j]] += 1
            if available[path[j]] >= counts[path[j]]:
                break
        if best is not None:
            for vertex in path[i + 1:best]:
                counts[vertex] -= 1
            del path[i + 1:best]
            i = max(0, i - 1)
        else:
            i += 1
    return path


def rotate_improve(path, adjacency, setup, required, rng, iterations):
    """Change endpoint without changing vertices, then remove revisits."""
    path = shorten_walk(list(path), adjacency)
    best = list(path)
    best_score = required * len(best) + min(setup[best[0]], setup[best[-1]])
    target_size = len(adjacency)
    lower_bound = required * target_size + min(setup)
    for iteration in range(iterations):
        if best_score == lower_bound:
            break
        if rng.randrange(2):
            path.reverse()
        endpoint = path[-1]
        choices = [i for i, vertex in enumerate(path[:-2])
                   if vertex in adjacency[endpoint]]
        if not choices:
            continue
        pivot = rng.choice(choices)
        path[pivot + 1:] = reversed(path[pivot + 1:])
        if len(path) > target_size:
            path = shorten_walk(path, adjacency)
        score = required * len(path) + min(setup[path[0]], setup[path[-1]])
        if score < best_score:
            best, best_score = list(path), score
        if iteration % 100 == 99 and len(path) > len(best):
            path = list(best)
    if setup[best[-1]] < setup[best[0]]:
        best.reverse()
    return best


def generate_candidate(radius, board, required, attempts=8, incumbent=None):
    """Return a deterministic improved target-only two-beam candidate."""
    from level6 import source_offsets, cover_walk, emit_walk

    target = [(r, c) for r, row in enumerate(board)
              for c, value in enumerate(row) if value == 'M']
    sources = source_offsets(radius, target)
    rng = Random(6713 + radius * 17 + len(target))
    best = None
    best_score = len(incumbent) if incumbent is not None else float('inf')
    for emitters in ((0, 1), (0, 2), (1, 2)):
        coordinates = [(source[emitters[0]], source[emitters[1]]) for source in sources]
        lookup = {point: i for i, point in enumerate(coordinates)}
        graph = [[lookup[x + dx, y + dy]
                  for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                  if (dx or dy) and (x + dx, y + dy) in lookup]
                 for x, y in coordinates]
        adjacency = list(map(set, graph))
        setup = [max(abs(x), abs(y)) for x, y in coordinates]
        lower_bound = required * len(target) + min(setup)
        if lower_bound >= best_score:
            continue
        starts = sorted(range(len(target)), key=lambda i: (setup[i], len(graph[i])))
        incumbent_path = None
        if incumbent is not None:
            offsets = [0] * 6
            active = set()
            path = []
            visited = set()
            for tick in incumbent:
                for emitter, instruction in enumerate(tick):
                    if instruction == 'T':
                        active.symmetric_difference_update((emitter,))
                    elif instruction == 'F':
                        offsets[emitter] += 1
                    elif instruction == 'B':
                        offsets[emitter] -= 1
                if not active:
                    continue
                if active != set(emitters):
                    break
                state = tuple(offsets[emitter] for emitter in emitters)
                if state not in lookup:
                    break
                vertex = lookup[state]
                path.append(vertex)
                visited.add(vertex)
                if len(visited) == len(target):
                    incumbent_path = path
                    break
        if incumbent_path:
            path = rotate_improve(incumbent_path, adjacency, setup, required,
                                  Random(78123 + radius * 17 + emitters[0] * 6 + emitters[1]),
                                  max(100, len(target) * 10))
            score = setup[path[0]] + required * len(path)
            if score < best_score:
                best_score = score
                best = emit_walk(emitters, [coordinates[i] for i in path], required)
        for attempt in range(attempts):
            start = starts[attempt % len(starts)] if attempt < 4 else rng.randrange(len(target))
            path = cover_walk(graph, start, rng)
            path = rotate_improve(path, adjacency, setup, required, rng,
                                  max(100, len(target) * 10))
            score = setup[path[0]] + required * len(path)
            if score < best_score:
                best_score = score
                best = emit_walk(emitters, [coordinates[i] for i in path], required)
            if best_score == lower_bound:
                break
    return best
