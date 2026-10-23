"""Six-beam safe walks with instant activation changes."""

from itertools import product
from random import Random

from level6_multibeam import _geometry
from level6_sixbeam import _walk, _shorten_rotations


class ToggleGraph:
    def __init__(self, radius, board):
        self.radius = radius
        self.base = radius * 2 + 1
        self.strides = [self.base ** i for i in range(6)]
        self.space = self.base ** 6
        self.states = []
        self.codes = []
        self.masks = []
        self.lookup = {}
        self.cache = {}
        self.delta_cache = {}
        self.ab_cache = {}
        rays, self.target, self.halo = _geometry(radius, board)
        self.pairs = []
        for axis in range(3):
            table = []
            for active in range(4):
                for lo in range(self.base):
                    for hi in range(self.base):
                        if lo + hi >= self.base:
                            table.append(None)
                            continue
                        a = rays[axis][lo - radius] if active & 1 else 0
                        b = rays[axis][radius - hi] if active & 2 else 0
                        table.append((a ^ b, a & b))
            self.pairs.append(table)

    def add_code(self, code):
        present = self.lookup.get(code)
        if present is not None:
            return present
        active, remaining = divmod(code, self.space)
        digits = []
        for _ in range(6):
            remaining, value = divmod(remaining, self.base)
            digits.append(value)
        pair_size = self.base * self.base
        ai = digits[0] * self.base + digits[3] + ((active & 1) | ((active >> 2) & 2)) * pair_size
        bi = digits[1] * self.base + digits[4] + (((active >> 1) & 1) | ((active >> 3) & 2)) * pair_size
        ci = digits[2] * self.base + digits[5] + (((active >> 2) & 1) | ((active >> 4) & 2)) * pair_size
        a, b, c = self.pairs[0][ai], self.pairs[1][bi], self.pairs[2][ci]
        if a is None or b is None or c is None:
            self.lookup[code] = -1
            return -1
        ab_key = ai * pair_size * 4 + bi
        combined = self.ab_cache.get(ab_key)
        if combined is None:
            a1, a2 = a
            b1, b2 = b
            aa, bb = a1 | a2, b1 | b2
            over = (a2 & bb) | (b2 & aa)
            if over & self.target:
                combined = False
            else:
                two = (a2 & ~bb) | (b2 & ~aa) | (a1 & b1)
                one = (a1 & ~bb) | (b1 & ~aa)
                combined = (aa | bb, one, two)
            self.ab_cache[ab_key] = combined
        if combined is False:
            self.lookup[code] = -1
            return -1
        ab, one, two = combined
        c1, c2 = c
        cc = c1 | c2
        if ((two & cc) | (one & c2)) & self.target:
            self.lookup[code] = -1
            return -1
        created = (two & ~cc) | (one & c1) | (c2 & ~ab)
        if created & self.halo:
            self.lookup[code] = -1
            return -1
        index = len(self.states)
        self.lookup[code] = index
        self.codes.append(code)
        self.states.append(tuple(v - self.radius for v in digits) + (active,))
        self.masks.append(created & self.target)
        return index

    def add_state(self, state, active=63):
        code = sum((v + self.radius) * s for v, s in zip(state, self.strides))
        return self.add_code(code + active * self.space)

    def __getitem__(self, index):
        if index in self.cache:
            return self.cache[index]
        state = self.states[index]
        active = state[6]
        boundary = tuple(-1 if v == -self.radius else 1 if v == self.radius else 0 for v in state[:6])
        deltas = self.delta_cache.get(boundary)
        if deltas is None:
            choices = [(0, 1) if flag == -1 else (-1, 0) if flag == 1 else (-1, 0, 1)
                       for flag in boundary]
            deltas = [sum(step * stride for step, stride in zip(change, self.strides))
                      for change in product(*choices) if any(change)]
            self.delta_cache[boundary] = deltas
        code = self.codes[index]
        neighbours = [n for delta in deltas if (n := self.add_code(code + delta)) >= 0]
        base_code = code - active * self.space
        neighbours.extend(n for a in range(64) if a != active
                          and (n := self.add_code(base_code + a * self.space)) >= 0)
        self.cache[index] = neighbours
        return neighbours


def _emit(states, path, required):
    setup = lambda p: max(map(abs, states[p][:6]))
    if setup(path[-1]) < setup(path[0]):
        path = list(reversed(path))
    first = states[path[0]]
    program = []
    for t in range(max(map(abs, first[:6]))):
        program.append(tuple(('F' if v > 0 else 'B') if t < abs(v) else '-'
                             for v in first[:6]))
    program.append(tuple('T' if first[6] & (1 << e) else '-' for e in range(6)))
    for repetition in range(required):
        if repetition:
            program.append(tuple('------'))
            path = list(reversed(path))
        for before, after in zip(path, path[1:]):
            a, b = states[before], states[after]
            if a[6] != b[6]:
                assert a[:6] == b[:6]
                program.append(tuple('T' if (a[6] ^ b[6]) & (1 << e) else '-'
                                     for e in range(6)))
            else:
                program.append(tuple('F' if y > x else 'B' if y < x else '-'
                                     for x, y in zip(a[:6], b[:6])))
    return program


def _bridge(graph, before, after):
    old, new = graph.states[before], graph.states[after]
    if old[:6] == new[:6]:
        return [after]
    extension = []
    position = list(old[:6])
    if old[6]:
        extension.append(graph.add_state(position, active=0))
    while tuple(position) != new[:6]:
        position = [x + (1 if y > x else -1 if y < x else 0)
                    for x, y in zip(position, new[:6])]
        node = graph.add_state(position, active=0)
        assert node >= 0
        extension.append(node)
    if new[6]:
        extension.append(after)
    return extension


def _walk_with_bridges(graph, starts, start, rng, variant, limit):
    path = [start]
    masks = graph.masks
    unseen = graph.target & ~masks[start]
    while unseen and len(path) < limit:
        current = path[-1]
        useful = [n for n in graph[current] if masks[n] & unseen]
        if useful:
            largest = max((masks[n] & unseen).bit_count() for n in useful)
            shortlist = [n for n in useful if (masks[n] & unseen).bit_count() >= largest - variant]
            if len(shortlist) > 8:
                rng.shuffle(shortlist)
                shortlist = shortlist[:8]
            def score(node):
                direct = (masks[node] & unseen).bit_count()
                future = max(((masks[n] & unseen & ~masks[node]).bit_count()
                              for n in graph[node]), default=0)
                return direct + 0.5 * future, rng.random()
            extension = [max(shortlist, key=score)]
        else:
            parents = {current: None}
            frontier = [current]
            nearby = None
            for depth in range(3):
                following, found = [], []
                for node in frontier:
                    for n in graph[node]:
                        if n in parents:
                            continue
                        parents[n] = node
                        following.append(n)
                        if masks[n] & unseen:
                            found.append(n)
                if found:
                    nearby = max(found, key=lambda n: ((masks[n] & unseen).bit_count(), rng.random()))
                    break
                if len(parents) > 3000:
                    break
                frontier = following
            if nearby is not None:
                extension = [nearby]
                while parents[extension[-1]] != current:
                    extension.append(parents[extension[-1]])
                extension.reverse()
                path.extend(extension)
                for n in extension:
                    unseen &= ~masks[n]
                continue
            candidates = [n for n in starts if masks[n] & unseen]
            if not candidates:
                return None
            position = graph.states[current]
            def travel_score(n):
                dist = max(abs(a - b) for a, b in zip(position[:6], graph.states[n][:6]))
                cost = 1 if not dist else dist + 2
                return (masks[n] & unseen).bit_count() / cost, rng.random()
            destination = max(candidates, key=travel_score)
            extension = _bridge(graph, current, destination)
        path.extend(extension)
        for n in extension:
            unseen &= ~masks[n]
    return path if not unseen else None


def generate_candidate(radius, board, required, attempts=6, budget=None,
                       samples=3000, rotations=30, bridges=True):
    graph = ToggleGraph(radius, board)
    from level6_factor import sample_configs
    states, _, _ = sample_configs(radius, board, samples=samples, top=120)
    starts = []
    for state in states:
        index = graph.add_state(state)
        if index >= 0 and graph.masks[index]:
            starts.append(index)
    starts.sort(key=lambda i: max(map(abs, graph.states[i][:6])) - 2 * graph.masks[i].bit_count())
    rng = Random(63477 + radius + graph.target)
    best, best_score = None, budget if budget is not None else float('inf')
    for attempt in range(attempts):
        if not starts:
            break
        start = starts[attempt % len(starts)] if attempt < 8 else rng.choice(starts)
        limit = ((best_score - max(map(abs, graph.states[start][:6]))) // required + 8
                 if best_score < float('inf') else graph.target.bit_count() * 4 + radius * 4)
        path = (_walk_with_bridges(graph, starts, start, rng, attempt % 2, limit)
                if bridges else _walk(graph.masks, graph, graph.target, start,
                                      rng, attempt % 2, limit))
        if path is None:
            continue
        if rotations:
            path = _shorten_rotations(path, graph.masks, graph, rng, rotations)
        program = _emit(graph.states, path, required)
        if len(program) < best_score:
            best, best_score = program, len(program)
    return best
