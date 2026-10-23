"""Exhaustive small-board review against direct physical ray tracing."""
import sys
from collections import Counter
from itertools import product, combinations_with_replacement
sys.path.insert(0, 'python')
from level1 import render_board
from level2 import DIRECTIONS
from level4 import border_positions
from level6_factor import iter_safe_configurations
from level6_sixbeam import SafetyGraph

r = 2
template = render_board(r).splitlines()
cells = [(y, x) for y, row in enumerate(template) for x, ch in enumerate(row) if ch == 'O']
ids = {cell: i for i, cell in enumerate(cells)}
border = border_positions(r)
pairs = list(combinations_with_replacement(range(-r, r + 1), 2))
physical = {}
for a, b, c in product(pairs, repeat=3):
    state = (a[0], b[0], c[0], -a[1], -b[1], -c[1])
    hits = Counter()
    for emitter, offset in enumerate(state):
        y, x = border[(emitter * r + offset) % (6 * r)]
        dy, dx = DIRECTIONS[str(emitter + 1)]
        y += dy
        x += dx
        while 0 <= y < len(template) and 0 <= x < len(template[0]):
            if (y, x) in ids:
                hits[y, x] += 1
            y += dy
            x += dx
    paint = sum(1 << ids[cell] for cell, count in hits.items() if count == 2)
    erase = sum(1 << ids[cell] for cell, count in hits.items() if count >= 3)
    physical[state] = paint, erase

comparisons = edges = 0
for target in range(1, 1 << len(cells)):
    selected = {cell for cell, index in ids.items() if target & (1 << index)}
    halo_cells = {(y + dy, x + dx) for y, x in selected
                  for dy, dx in DIRECTIONS.values()} - selected
    halo = sum(1 << ids[cell] for cell in halo_cells if cell in ids)
    board = [''.join('M' if (y, x) in selected else ch for x, ch in enumerate(row))
             for y, row in enumerate(template)]
    expected = {state: paint & target for state, (paint, erase) in physical.items()
                if not paint & halo and not erase & target}
    actual = dict(iter_safe_configurations(r, board))
    assert actual == expected, target
    graph = SafetyGraph(r, board)
    for state, (paint, erase) in physical.items():
        index = graph.add_state(state)
        assert (index >= 0) == (state in expected), (target, state)
        if index >= 0:
            assert graph.masks[index] == expected[state], (target, state)
        comparisons += 1
    if target in (1, 5, 63, 127):
        for state in list(expected)[::max(1, len(expected) // 12)]:
            index = graph.add_state(state)
            want = {other for other in expected if other != state
                    and all(abs(a - b) <= 1 for a, b in zip(state, other))}
            got = {graph.states[index] for index in graph[index]}
            assert got == want, (target, state)
            edges += len(got)

for a, b in product(range(-r, r + 1), repeat=2):
    for da, db in product((-1, 0, 1), repeat=2):
        if -r <= a + da <= r and -r <= b + db <= r:
            before, after = sorted((a, b)), sorted((a + da, b + db))
            assert all(abs(x - y) <= 1 for x, y in zip(before, after))

print('PASS', comparisons, 'canonical configurations across all 127 nonempty radius-2 targets;',
      edges, 'neighbor links checked; sorting preserves one-tick motion.')
