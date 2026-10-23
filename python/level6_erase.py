"""Fill whole lines, then clear their halo with up to six moving beams."""

from random import Random
from itertools import product
from time import monotonic

from level6_sixbeam import SafetyGraph


class EraseGraph(SafetyGraph):
    def add_code(self, code):
        is_new = code not in self.lookup
        index = super().add_code(code)
        if index >= 0 and is_new:
            s = self.states[index]
            d = [v + self.radius for v in s]
            a1, a2 = self.pairs[0][d[0] * self.base + d[3]]
            b1, b2 = self.pairs[1][d[1] * self.base + d[4]]
            c1, c2 = self.pairs[2][d[2] * self.base + d[5]]
            aa, bb, cc = a1 | a2, b1 | b2, c1 | c2
            over = (a2 & bb) | (b2 & aa)
            two = (a2 & ~bb) | (b2 & ~aa) | (a1 & b1)
            one = (a1 & ~bb) | (b1 & ~aa)
            self.masks[index] = (over | (two & cc) | (one & c2)) & self.halo
        return index


def _walk(graph, goal, start, rng, limit, deadline):
    path = [start]
    unseen = goal & ~graph.masks[start]
    while unseen and len(path) < limit and monotonic() < deadline:
        current = path[-1]
        choices = [n for n in graph[current] if graph.masks[n] & unseen]
        if choices:
            extension = [max(choices, key=lambda n: ((graph.masks[n] & unseen).bit_count(), rng.random()))]
        else:
            parents = {current: None}
            frontier = [current]
            found = []
            while frontier and not found and len(parents) < 4000:
                following = []
                for node in frontier:
                    if monotonic() >= deadline:
                        return None
                    for nxt in graph[node]:
                        if nxt in parents:
                            continue
                        parents[nxt] = node
                        following.append(nxt)
                        if graph.masks[nxt] & unseen:
                            found.append(nxt)
                frontier = following
            if not found:
                return None
            destination = max(found, key=lambda n: ((graph.masks[n] & unseen).bit_count(), rng.random()))
            extension = [destination]
            while parents[extension[-1]] != current:
                extension.append(parents[extension[-1]])
            extension.reverse()
        path.extend(extension)
        for node in extension:
            unseen &= ~graph.masks[node]
    return path if not unseen else None


def _emit(radius, axis, lines, states, required):
    from level6 import move_command
    program, position, active = [], [0] * 6, set()

    def move_to(destination, enabled):
        if active:
            program.append(tuple('T' if i in active else '-' for i in range(6)))
            active.clear()
        deltas = [b - a for a, b in zip(position, destination)]
        for t in range(max(map(abs, deltas))):
            program.append(tuple(move_command(d) if t < abs(d) else '-' for d in deltas))
        position[:] = destination
        program.append(tuple('T' if i in enabled else '-' for i in range(6)))
        active.update(enabled)

    for repetition in range(required):
        fill = list(lines)
        def distance(line):
            return max(abs(position[axis] - line), abs(position[axis + 3] + line))
        if distance(fill[-1]) < distance(fill[0]):
            fill.reverse()
        destination = list(position)
        destination[axis], destination[axis + 3] = fill[0], -fill[0]
        move_to(destination, (axis, axis + 3))
        for line in fill[1:]:
            operations = ['-'] * 6
            operations[axis] = move_command(line - position[axis])
            operations[axis + 3] = move_command(-line - position[axis + 3])
            assert abs(line - position[axis]) <= 1
            program.append(tuple(operations))
            position[axis], position[axis + 3] = line, -line
        path = states
        if max(abs(a - b) for a, b in zip(position, path[-1])) < max(abs(a - b) for a, b in zip(position, path[0])):
            path = path[::-1]
        move_to(path[0], range(6))
        for before, after in zip(path, path[1:]):
            program.append(tuple(move_command(b - a) for a, b in zip(before, after)))
        position[:] = path[-1]
    return program


def generate_candidate(radius, board, required, budget=None, attempts=8, seconds=6):
    from level6 import source_offsets
    from level6_multibeam import _geometry
    from level5 import simulate
    deadline = monotonic() + seconds
    cells = [(r, c) for r, row in enumerate(board) for c, ch in enumerate(row) if ch in 'OM']
    offsets = source_offsets(radius, cells)
    rays, target, halo = _geometry(radius, board)
    if not halo:
        return None
    choices = []
    for axis in range(3):
        lines = sorted({s[axis] for i, s in enumerate(offsets) if target & (1 << i)})
        painted = 0
        for line in lines:
            painted |= rays[axis][line]
        goal = painted & halo
        if goal:
            choices.append((len(lines) + goal.bit_count() / 3, axis, lines, goal))
    if not choices:
        return None
    choices.sort()
    graph = EraseGraph(radius, board)
    halo_offsets = [s[:3] for i, s in enumerate(offsets) if halo & (1 << i)]
    starts = set()
    for point in halo_offsets:
        for empty in product((-radius, radius), repeat=3):
            lo = tuple(min(a, b) for a, b in zip(point, empty))
            hi = tuple(-max(a, b) for a, b in zip(point, empty))
            node = graph.add_state(lo + hi)
            if node >= 0 and graph.masks[node]:
                starts.add(node)
    rng = Random(41673 + radius + target)
    for _ in range(1500):
        first, second = rng.choice(halo_offsets), rng.choice(halo_offsets)
        lo = tuple(min(a, b) for a, b in zip(first, second))
        hi = tuple(-max(a, b) for a, b in zip(first, second))
        node = graph.add_state(lo + hi)
        if node >= 0 and graph.masks[node]:
            starts.add(node)
    best, best_score = None, budget if budget is not None else float('inf')
    for _, axis, lines, goal in choices:
        ranked = sorted(starts, key=lambda n: (-(graph.masks[n] & goal).bit_count(),
                                               max(map(abs, graph.states[n]))))
        for attempt in range(attempts):
            if monotonic() >= deadline:
                return best
            start = ranked[attempt % len(ranked)] if attempt < 4 else rng.choice(ranked[:max(10, len(ranked) // 3)])
            limit = int(best_score / required - len(lines) - 3) if best_score != float('inf') else goal.bit_count() * 3
            path = _walk(graph, goal, start, rng, limit, deadline)
            if path is None:
                continue
            program = _emit(radius, axis, lines, [graph.states[n] for n in path], required)
            actual = simulate(radius, board, program)
            if actual < required:
                continue
            if actual > required:
                lo, hi = 1, len(program)
                while lo < hi:
                    mid = (lo + hi) // 2
                    if simulate(radius, board, program[:mid]) >= required:
                        hi = mid
                    else:
                        lo = mid + 1
                program = program[:lo]
            if len(program) < best_score:
                best, best_score = program, len(program)
    return best
