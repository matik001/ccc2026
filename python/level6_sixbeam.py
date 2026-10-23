"""Walk a sparse graph of safe six-emitter configurations."""

from itertools import product
from random import Random

class LazyGraph:
    def __init__(self, states, radius):
        self.states = states
        self.radius = radius
        self.base = radius * 2 + 1
        self.strides = [self.base ** i for i in range(6)]
        self.codes = [sum((v + radius) * stride for v, stride in zip(s, self.strides))
                      for s in states]
        self.lookup = {code: i for i, code in enumerate(self.codes)}
        self.cache = {}
        self.delta_cache = {}

    def __getitem__(self, index):
        if index in self.cache:
            return self.cache[index]
        state = self.states[index]
        boundary = tuple(-1 if v == -self.radius else 1 if v == self.radius else 0 for v in state)
        deltas = self.delta_cache.get(boundary)
        if deltas is None:
            choices = [(0, 1) if flag == -1 else (-1, 0) if flag == 1 else (-1, 0, 1)
                       for flag in boundary]
            deltas = [sum(step * stride for step, stride in zip(change, self.strides))
                      for change in product(*choices) if any(change)]
            self.delta_cache[boundary] = deltas
        code = self.codes[index]
        get = self.lookup.get
        neighbours = [n for delta in deltas if (n := get(code + delta)) is not None]
        self.cache[index] = neighbours
        return neighbours


class SafetyGraph(LazyGraph):
    """Discover safe vertices as needed, without enumerating millions of states."""
    def __init__(self, radius, board):
        super().__init__([], radius)
        from level6_multibeam import _geometry
        rays, self.target, self.halo = _geometry(radius, board)
        self.masks = []
        self.pairs = []
        for axis in range(3):
            table = []
            for lo in range(self.base):
                for hi in range(self.base):
                    if lo + hi >= self.base:
                        table.append(None)
                        continue
                    a, b = rays[axis][lo - radius], rays[axis][radius - hi]
                    table.append((a ^ b, a & b))
            self.pairs.append(table)
        self.ab_cache = {}

    def add_code(self, code):
        present = self.lookup.get(code)
        if present is not None:
            return present
        remaining = code
        digits = []
        for _ in range(6):
            remaining, value = divmod(remaining, self.base)
            digits.append(value)
        ai = digits[0] * self.base + digits[3]
        bi = digits[1] * self.base + digits[4]
        ci = digits[2] * self.base + digits[5]
        a, b, c = self.pairs[0][ai], self.pairs[1][bi], self.pairs[2][ci]
        if a is None or b is None or c is None:
            self.lookup[code] = -1
            return -1
        ab_key = ai * self.base * self.base + bi
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
        self.states.append(tuple(v - self.radius for v in digits))
        self.masks.append(created & self.target)
        return index

    def add_state(self, state):
        return self.add_code(sum((v + self.radius) * s for v, s in zip(state, self.strides)))

    def __getitem__(self, index):
        if index in self.cache:
            return self.cache[index]
        state = self.states[index]
        boundary = tuple(-1 if v == -self.radius else 1 if v == self.radius else 0 for v in state)
        deltas = self.delta_cache.get(boundary)
        if deltas is None:
            choices = [(0, 1) if flag == -1 else (-1, 0) if flag == 1 else (-1, 0, 1)
                       for flag in boundary]
            deltas = [sum(step * stride for step, stride in zip(change, self.strides))
                      for change in product(*choices) if any(change)]
            self.delta_cache[boundary] = deltas
        code = self.codes[index]
        neighbours = [n for delta in deltas if (n := self.add_code(code + delta)) >= 0]
        self.cache[index] = neighbours
        return neighbours


def _walk(masks, graph, target, start, rng, variant, limit):
    path = [start]
    unseen = target & ~masks[start]
    while unseen and len(path) < limit:
        current = path[-1]
        useful = [n for n in graph[current] if masks[n] & unseen]
        if useful:
            largest = max((masks[n] & unseen).bit_count() for n in useful)
            shortlist = [n for n in useful if (masks[n] & unseen).bit_count() >= largest - variant]
            if len(shortlist) > 12:
                rng.shuffle(shortlist)
                shortlist = shortlist[:12]
            def score(node):
                direct = (masks[node] & unseen).bit_count()
                future = max(((masks[n] & unseen & ~masks[node]).bit_count()
                              for n in graph[node]), default=0)
                return direct + (0.4 if variant else 0.65) * future, rng.random()
            extension = [max(shortlist, key=score)]
        else:
            parents = {current: None}
            frontier = [current]
            destination = None
            while frontier and destination is None and len(parents) < 8000:
                following, found = [], []
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
                    destination = max(found, key=lambda n: ((masks[n] & unseen).bit_count(), rng.random()))
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
    return path if not unseen else None


def _emit(states, path, required):
    if max(map(abs, states[path[-1]])) < max(map(abs, states[path[0]])):
        path = list(reversed(path))
    first = states[path[0]]
    program = []
    for t in range(max(map(abs, first))):
        program.append(tuple(('F' if v > 0 else 'B') if t < abs(v) else '-'
                             for v in first))
    program.append(tuple('TTTTTT'))
    for repetition in range(required):
        if repetition:
            program.append(tuple('------'))
            path = list(reversed(path))
        for before, after in zip(path, path[1:]):
            program.append(tuple('F' if y > x else 'B' if y < x else '-'
                                 for x, y in zip(states[before], states[after])))
    return program


def _shorten_rotations(path, masks, graph, rng, iterations=200):
    path = list(path)
    target = 0
    for n in path:
        target |= masks[n]
    for _ in range(iterations):
        if rng.randrange(2):
            path.reverse()
        suffix = [0] * (len(path) + 1)
        for i in range(len(path) - 1, -1, -1):
            suffix[i] = suffix[i + 1] | masks[path[i]]
        while len(path) > 1 and suffix[1] == target:
            path.pop(0)
            suffix.pop(0)
        prefix = 0
        for i in range(len(path) - 1):
            prefix |= masks[path[i]]
            if prefix == target:
                del path[i + 1:]
                break
            neighbours = set(graph[path[i]])
            for j in range(len(path) - 1, i + 1, -1):
                if prefix | suffix[j] == target and (path[j] in neighbours or path[j] == path[i]):
                    del path[i + 1:j]
                    break
            else:
                continue
            break
        if len(path) < 3:
            break
        neighbours = set(graph[path[-1]])
        pivots = [i for i in range(len(path) - 2)
                  if path[i] in neighbours or path[i] == path[-1]]
        if pivots:
            pivot = rng.choice(pivots)
            path[pivot + 1:] = reversed(path[pivot + 1:])
    return path


def generate_from_configs(radius, required, states, masks, target,
                          attempts=12, budget=None, rotations=200):
    if not states:
        return None
    graph = LazyGraph(states, radius)
    return _generate_from_graph(radius, required, states, masks, target, graph,
                                attempts, budget, rotations)


def _generate_from_graph(radius, required, states, masks, target, graph,
                         attempts, budget, rotations, start_indices=None):
    starts = [i for i, m in enumerate(masks) if m] if start_indices is None else list(start_indices)
    starts.sort(key=lambda i: (max(map(abs, states[i])) - 2 * masks[i].bit_count(),
                               -masks[i].bit_count()))
    if not starts:
        return None
    rng = Random(53873 + radius + target)
    best_path = None
    best_score = budget if budget is not None else float('inf')
    for attempt in range(attempts):
        start = starts[attempt % len(starts)] if attempt < 8 else rng.choice(starts)
        # Avoid computing global rarity weights over a potentially huge graph.
        limit = ((best_score - max(map(abs, states[start]))) // required + 8
                 if best_score < float('inf') else target.bit_count() * 4 + radius * 4)
        path = _walk(masks, graph, target, start, rng, attempt % 2, limit)
        if path is None:
            continue
        if rotations:
            path = _shorten_rotations(path, masks, graph, rng, rotations)
        score = required * len(path) + min(max(map(abs, states[path[0]])),
                                           max(map(abs, states[path[-1]])))
        if score < best_score:
            best_path, best_score = path, score
    return _emit(states, best_path, required) if best_path is not None else None


def generate_candidate(radius, board, required, attempts=12, budget=None,
                       rotations=200):
    if radius <= 5:
        from level6_factor import enumerate_configs
        states, masks, target = enumerate_configs(radius, board)
        return generate_from_configs(radius, required, states, masks, target,
                                     attempts, budget, rotations)
    graph = SafetyGraph(radius, board)
    from level6_factor import sample_configs
    sampled_states, _, _ = sample_configs(radius, board, samples=6000, top=120)
    starts = []
    for state in sampled_states:
        index = graph.add_state(state)
        if index >= 0 and graph.masks[index]:
            starts.append(index)
    return _generate_from_graph(radius, required, graph.states, graph.masks,
                                graph.target, graph, attempts, budget, rotations,
                                starts)
