"""Independent smoke checks of candidates on random connected shapes."""
import json
import random
import time
from collections import Counter
from pathlib import Path

from level1 import render_board
from level2 import DIRECTIONS
from level5 import simulate
import level6_multibeam
import level6_static
import level6_dense

rng = random.Random(237771)
counts = Counter()
started = time.monotonic()
for iteration in range(180):
    radius = 1 + iteration % 6
    original = render_board(radius).splitlines()
    cells = {(r, c) for r, row in enumerate(original) for c, ch in enumerate(row) if ch == 'O'}
    desired = rng.randrange(1, len(cells) + 1)
    target = {rng.choice(sorted(cells))}
    while len(target) < desired:
        fringe = {(r + dr, c + dc) for r, c in target for dr, dc in DIRECTIONS.values()} & cells - target
        target.add(rng.choice(sorted(fringe)))
    board = [''.join('M' if (r, c) in target else ch for c, ch in enumerate(row))
             for r, row in enumerate(original)]
    required = rng.randrange(1, 7)
    candidates = {
        'multibeam': lambda: level6_multibeam.generate_candidate(radius, board, required,
                                                               attempts=3, rotations=160),
        'static': lambda: level6_static.generate_candidate(radius, board, required),
        'dense': lambda: level6_dense.generate_candidate(radius, board, required, attempts=3),
    }
    for name, make in candidates.items():
        try:
            candidate = make()
            if candidate is not None:
                actual = simulate(radius, board, candidate)
                assert actual == required, (actual, required)
                counts[name] += 1
        except Exception as exc:
            Path(__file__).with_name('route_review_failure.json').write_text(json.dumps(
                [iteration, radius, board, required, name, str(exc)]))
            raise
    if iteration % 30 == 0:
        print(iteration, dict(counts), round(time.monotonic() - started, 2), flush=True)
print('PASS', 180, dict(counts), round(time.monotonic() - started, 2), flush=True)
