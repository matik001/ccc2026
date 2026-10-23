"""Dense patterns: fill the board, then erase the surrounding cells."""

from collections import deque
from random import Random

from level2 import DIRECTIONS
from level4 import border_positions
from level5 import simulate


def _delta(a, b, length):
    d = (b - a) % length
    return d if d <= length // 2 else d - length


class _Plan:
    def __init__(self, radius):
        self.radius = radius
        self.positions = [i * radius for i in range(6)]
        self.active = set()
        self.program = []

    def tick(self, operations):
        self.program.append(tuple(operations))
        for i, op in enumerate(operations):
            if op in ('F', 'B'):
                self.positions[i] = (self.positions[i] + (1 if op == 'F' else -1)) % (6 * self.radius)
            elif op == 'T':
                self.active.symmetric_difference_update({i})

    def off(self):
        if self.active:
            self.tick(['T' if i in self.active else '-' for i in range(6)])

    def go(self, emitters, sources):
        assert not self.active
        deltas = [_delta(self.positions[i], sources[i], 6 * self.radius) for i in emitters]
        for t in range(max(map(abs, deltas), default=0)):
            tick = ['-'] * 6
            for i, d in zip(emitters, deltas):
                if t < abs(d):
                    tick[i] = 'F' if d > 0 else 'B'
            self.tick(tick)
        self.tick(['T' if i in emitters else '-' for i in range(6)])

    def step(self, emitters, sources):
        tick = ['-'] * 6
        for i in emitters:
            d = _delta(self.positions[i], sources[i], 6 * self.radius)
            assert abs(d) <= 1
            tick[i] = 'F' if d > 0 else 'B' if d < 0 else '-'
        self.tick(tick)


def generate_candidate(radius, board, required, attempts=6, budget=None):
    cells = {(r, c) for r, row in enumerate(board) for c, ch in enumerate(row) if ch in 'OM'}
    target = {(r, c) for r, c in cells if board[r][c] == 'M'}
    halo = ({(r + dr, c + dc) for r, c in target for dr, dc in DIRECTIONS.values()} & cells) - target
    if not halo:
        return None
    rail = {p: i for i, p in enumerate(border_positions(radius))}
    sources = {}
    for r, c in sorted(cells):
        positions = []
        for dr, dc in DIRECTIONS.values():
            y, x = r, c
            while (y, x) not in rail:
                y, x = y - dr, x - dc
            positions.append(rail[y, x])
        sources[r, c] = positions
    empty = cells - target
    graph = {p: [(p[0] + dr, p[1] + dc) for dr, dc in DIRECTIONS.values()
                 if (p[0] + dr, p[1] + dc) in empty] for p in sorted(empty)}
    best = None
    best_score = budget or float('inf')
    rng = Random(radius + len(target) * 97)
    for attempt in range(attempts):
        axis = attempt % 3
        lines = {}
        for p in sorted(cells):
            lines[_delta(axis * radius, sources[p][axis], 6 * radius)] = p
        selected = {_delta(axis * radius, sources[p][axis], 6 * radius) for p in target}
        relevant_halo = {p for p in halo
                         if _delta(axis * radius, sources[p][axis], 6 * radius) in selected}
        if required * (len(selected) + len(relevant_halo)) >= best_score:
            continue
        fill = [lines[k] for k in sorted(selected, reverse=bool(attempt // 3 % 2))]
        plan = _Plan(radius)
        for _ in range(required):
            plan.off()
            plan.go((axis, axis + 3), sources[fill[0]])
            for p in fill[1:]:
                plan.step((axis, axis + 3), sources[p])
            if not relevant_halo:
                continue
            plan.off()
            pending = set(relevant_halo)
            current = None
            while pending:
                if current is None:
                    current = min(sorted(pending), key=lambda p: (
                        max(abs(_delta(plan.positions[i], sources[p][i], 6 * radius)) for i in range(3)),
                        rng.random()))
                    plan.go((0, 1, 2), sources[current])
                    pending.discard(current)
                    continue
                neighbours = [p for p in graph[current] if p in pending]
                if neighbours:
                    next_cell = min(neighbours, key=lambda p: (
                        sum(q in pending for q in graph[p]), rng.random()))
                    path = [next_cell]
                else:
                    queue = deque([current])
                    parent = {current: None}
                    next_cell = None
                    while queue and next_cell is None:
                        node = queue.popleft()
                        for p in graph[node]:
                            if p in parent:
                                continue
                            parent[p] = node
                            if p in pending:
                                next_cell = p
                                break
                            queue.append(p)
                    if next_cell is None:
                        plan.off()
                        current = None
                        continue
                    path = [next_cell]
                    while parent[path[-1]] != current:
                        path.append(parent[path[-1]])
                    path.reverse()
                for p in path:
                    plan.step((0, 1, 2), sources[p])
                    pending.discard(p)
                current = path[-1]
        if len(plan.program) < best_score and simulate(radius, board, plan.program) == required:
            best, best_score = plan.program, len(plan.program)
    return best
