"""Search safe three-beam configurations, covering several target cells per tick."""

from collections import deque
from itertools import product
from random import Random

from level2 import DIRECTIONS
from level4 import border_positions


def _geometry(radius, board):
    cells = [(r, c) for r, line in enumerate(board)
             for c, symbol in enumerate(line) if symbol in 'OM']
    target_cells = {p for p in cells if board[p[0]][p[1]] == 'M'}
    forbidden_cells = {(r + dr, c + dc) for r, c in target_cells
                       for dr, dc in DIRECTIONS.values()} - target_cells
    target = sum(1 << i for i, p in enumerate(cells) if p in target_cells)
    forbidden = sum(1 << i for i, p in enumerate(cells) if p in forbidden_cells)
    border = border_positions(radius)
    rail = {p: i for i, p in enumerate(border)}
    rays = [{v: 0 for v in range(-radius, radius + 1)} for _ in range(3)]
    for i, (row, col) in enumerate(cells):
        for axis, (dr, dc) in enumerate(list(DIRECTIONS.values())[:3]):
            r, c = row, col
            while (r, c) not in rail:
                r -= dr
                c -= dc
            value = (rail[r, c] - axis * radius) % (6 * radius)
            if value > 3 * radius:
                value -= 6 * radius
            rays[axis][value] |= 1 << i
    return rays, target, forbidden


def _graph(radius, board):
    rays, target, forbidden = _geometry(radius, board)
    states = []
    masks = []
    lookup = {}
    for x, a in rays[0].items():
        for y, b in rays[1].items():
            ab = a & b
            for z, c in rays[2].items():
                destroy = ab & c
                if destroy & target:
                    continue
                create = (ab | (a & c) | (b & c)) ^ destroy
                if create & forbidden:
                    continue
                state = (x, y, z)
                lookup[state] = len(states)
                states.append(state)
                masks.append(create & target)
    changes = [d for d in product((-1, 0, 1), repeat=3) if any(d)]
    graph = [[lookup[point] for dx, dy, dz in changes
              if (point := (x + dx, y + dy, z + dz)) in lookup]
             for x, y, z in states]
    return states, masks, graph, target


def _walk(states, masks, graph, target, start, rng, variant=0):
    path = [start]
    unseen = target & ~masks[start]
    # A scarcity weight prevents leaving isolated cells behind at the end.
    weights = {}
    if variant % 2:
        for m in masks:
            while m:
                b = m & -m
                weights[b] = weights.get(b, 0) + 1
                m -= b
    def gain(mask):
        mask &= unseen
        if not weights:
            return mask.bit_count()
        value = 0
        while mask:
            b = mask & -mask
            value += 1 + 8 / weights[b]
            mask -= b
        return value
    while unseen:
        current = path[-1]
        options = [n for n in graph[current] if masks[n] & unseen]
        if options:
            def score(node):
                direct = gain(masks[node])
                future = max((gain(masks[n] & ~masks[node]) for n in graph[node]), default=0)
                return (direct + future * (0.25 if variant < 2 else 0.55), rng.random())
            destination = max(options, key=score)
            extension = [destination]
        else:
            parents = {current: None}
            frontier = [current]
            destination = None
            while frontier and destination is None:
                following = []
                found = []
                rng.shuffle(frontier)
                for node in frontier:
                    for n in graph[node]:
                        if n in parents:
                            continue
                        parents[n] = node
                        following.append(n)
                        if masks[n] & unseen:
                            found.append(n)
                if found:
                    destination = max(found, key=lambda n: (gain(masks[n]), rng.random()))
                frontier = following
            if destination is None:
                return None
            extension = [destination]
            while parents[extension[-1]] != current:
                extension.append(parents[extension[-1]])
            extension.reverse()
        path.extend(extension)
        for node in extension:
            unseen &= ~masks[node]
    # Remove redundant initial states and shortcut interior revisits.
    counts = {}
    for node in path:
        m = masks[node]
        while m:
            b = m & -m
            counts[b] = counts.get(b, 0) + 1
            m -= b
    def removable(node):
        m = masks[node]
        while m:
            b = m & -m
            if counts[b] <= 1:
                return False
            m -= b
        return True
    def remove(node):
        m = masks[node]
        while m:
            b = m & -m
            counts[b] -= 1
            m -= b
    while len(path) > 1 and removable(path[0]):
        remove(path.pop(0))
    i = 1
    while i + 1 < len(path):
        if removable(path[i]) and path[i + 1] in graph[path[i - 1]]:
            remove(path.pop(i))
            i = max(1, i - 1)
        else:
            i += 1
    # A redundant detour can contain several points even if deleting any single
    # one would disconnect the walk. Shortcut whole intervals in one pass.
    changed = True
    while changed:
        changed = False
        suffix = [0] * (len(path) + 1)
        for i in range(len(path) - 1, -1, -1):
            suffix[i] = suffix[i + 1] | masks[path[i]]
        prefix = 0
        for i in range(len(path) - 2):
            prefix |= masks[path[i]]
            neighbours = graph[path[i]]
            for j in range(len(path) - 1, i + 1, -1):
                if prefix | suffix[j] != target:
                    continue
                if path[j] in neighbours or path[j] == path[i]:
                    del path[i + 1:j]
                    changed = True
                    break
            if changed:
                break
    return path


def _program(states, path, required):
    if max(map(abs, states[path[-1]])) < max(map(abs, states[path[0]])):
        path = list(reversed(path))
    first = states[path[0]]
    program = []
    for t in range(max(map(abs, first))):
        program.append(tuple(('F' if v > 0 else 'B') if t < abs(v) else '-'
                             for v in first) + ('-', '-', '-'))
    program.append(tuple('TTT---'))
    for repetition in range(required):
        if repetition:
            program.append(tuple('------'))
            path = list(reversed(path))
        for before, after in zip(path, path[1:]):
            program.append(tuple('F' if y > x else 'B' if y < x else '-'
                                 for x, y in zip(states[before], states[after]))
                           + ('-', '-', '-'))
    return program


def _rotate_shorten(path, states, masks, graph, target, required, rng, iterations):
    """Rotate either end of the walk, exposing redundant covered states."""
    path = list(path)
    counts = {}
    node_bits = []
    for mask in masks:
        bits = []
        while mask:
            b = mask & -mask
            bits.append(b)
            mask -= b
        node_bits.append(bits)
    for node in path:
        for b in node_bits[node]:
            counts[b] = counts.get(b, 0) + 1
    def removable(node):
        return all(counts[b] > 1 for b in node_bits[node])
    def discard(node):
        for b in node_bits[node]:
            counts[b] -= 1
    def score(p):
        return required * len(p) + min(max(map(abs, states[p[0]])),
                                       max(map(abs, states[p[-1]])))
    best = path[:]
    best_score = score(path)
    graph_sets = [set(neighbours) for neighbours in graph]
    for iteration in range(iterations):
        if rng.randrange(2):
            path.reverse()
        while len(path) > 1 and removable(path[0]):
            discard(path.pop(0))
        while len(path) > 1 and removable(path[-1]):
            discard(path.pop())
        i = 1
        while i + 1 < len(path):
            if removable(path[i]) and (path[i + 1] in graph_sets[path[i - 1]]
                                       or path[i + 1] == path[i - 1]):
                discard(path.pop(i))
                i = max(1, i - 1)
            else:
                i += 1
        candidate_score = score(path)
        if candidate_score < best_score:
            best, best_score = path[:], candidate_score
        if len(path) < 3:
            break
        neighbours = graph_sets[path[-1]]
        pivots = [i for i in range(len(path) - 2)
                  if path[i] in neighbours or path[i] == path[-1]]
        if pivots:
            pivot = rng.choice(pivots)
            path[pivot + 1:] = reversed(path[pivot + 1:])
    return best


def generate_candidate(radius, board, required, attempts=12, budget=None,
                       rotations=0):
    states, masks, graph, target = _graph(radius, board)
    starts = [i for i, m in enumerate(masks) if m]
    starts.sort(key=lambda i: (max(map(abs, states[i])) - 2 * masks[i].bit_count(),
                               -masks[i].bit_count()))
    if not starts:
        return None
    rng = Random(16423 + radius + target)
    best_path = None
    best_score = float('inf')
    for attempt in range(attempts):
        start = starts[attempt % len(starts)] if attempt < 8 else rng.choice(starts)
        path = _walk(states, masks, graph, target, start, rng, attempt % 4)
        if path is None:
            continue
        score = required * len(path) + min(max(map(abs, states[path[0]])),
                                           max(map(abs, states[path[-1]])))
        if score < best_score:
            best_path = path
            best_score = score
    if best_path is None:
        return None
    if rotations:
        best_path = _rotate_shorten(best_path, states, masks, graph, target,
                                    required, rng, rotations)
    program = _program(states, best_path, required)
    return program if budget is None or len(program) < budget else None



