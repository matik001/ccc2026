"""Cover a pattern with four or five independently moving beams."""

from itertools import product
from random import Random

from level6_multibeam import _geometry


def generate_candidate(radius, board, required, attempts=8, budget=None, beam_count=4, rotations=0):
    if beam_count not in (4, 5):
        raise ValueError('beam_count must be four or five')
    rays, target, halo = _geometry(radius, board)
    choices = tuple(range(-radius, radius + 1))
    best, best_score = None, budget or 10**9
    rng = Random(4177 + radius * 103 + target.bit_count())
    deltas = tuple(product((-1, 0, 1), repeat=beam_count))

    for axis in range(3):
        extra_axes = (axis,) if beam_count == 4 else (axis, (axis + 1) % 3)
        cache = {}
        edges = {}

        def mask(state):
            if state in cache:
                return cache[state]
            x, y, z = state[:3]
            a, b, c = rays[0][x], rays[1][y], rays[2][z]
            pairs = (a & b) | (a & c) | (b & c)
            one = a | b | c
            triple = a & b & c
            for extra, value in zip(extra_axes, state[3:]):
                d = rays[extra][value]
                triple |= pairs & d
                pairs |= one & d
                one |= d
            if triple & target:
                value = -1
            else:
                create = pairs & ~triple
                value = -1 if create & halo else create & target
            cache[state] = value
            return value

        def neighbours(state):
            if state in edges:
                return edges[state]
            options = []
            for change in deltas:
                nxt = tuple(x + d for x, d in zip(state, change))
                if min(nxt) < -radius or max(nxt) > radius or any(nxt[a] > nxt[3 + i] for i, a in enumerate(extra_axes)):
                    continue
                if nxt != state and mask(nxt) >= 0:
                    options.append(nxt)
            edges[state] = options
            return options

        starts = []
        for _ in range(1400):
            state = [rng.choice(choices) for _ in range(beam_count)]
            for i, a in enumerate(extra_axes):
                if state[a] > state[3 + i]:
                    state[a], state[3 + i] = state[3 + i], state[a]
            state = tuple(state)
            m = mask(state)
            if m > 0:
                starts.append(state)
        # Three beams remain available on the faces where the extra beam is off-board.
        for x, y, z in product(choices, repeat=3):
            if rng.random() > 0.03:
                continue
            state = (x, y, z) + (radius,) * (beam_count - 3)
            if mask(state) > 0:
                starts.append(state)
        starts = list(set(starts))
        starts.sort(key=lambda p: (max(map(abs, p)) - 2 * mask(p).bit_count(), rng.random()))
        if not starts:
            continue
        for attempt in range(attempts):
            start = starts[attempt % len(starts)] if attempt < 4 else rng.choice(starts[:max(10, len(starts)//3)])
            path = [start]
            unseen = target & ~mask(start)
            max_path = (best_score - max(map(abs, start))) // required
            while unseen and len(path) < max_path:
                current = path[-1]
                useful = [p for p in neighbours(current) if mask(p) & unseen]
                if useful:
                    # Prefer gaining several cells while retaining a productive next step.
                    scores = [(mask(p) & unseen).bit_count() for p in useful]
                    largest = max(scores)
                    shortlist = [p for p, value in zip(useful, scores) if value >= largest - (attempt % 2)]
                    def score(p):
                        direct = (mask(p) & unseen).bit_count()
                        future = max(((mask(q) & unseen & ~mask(p)).bit_count()
                                      for q in neighbours(p)), default=0)
                        return direct + (0.35 if attempt % 2 else 0.55) * future, rng.random()
                    extension = [max(shortlist, key=score)]
                else:
                    parents = {current: None}
                    frontier = [current]
                    destination = None
                    while frontier and destination is None:
                        following, found = [], []
                        rng.shuffle(frontier)
                        for p in frontier:
                            for q in neighbours(p):
                                if q in parents:
                                    continue
                                parents[q] = p
                                following.append(q)
                                if mask(q) & unseen:
                                    found.append(q)
                        if found:
                            destination = max(found, key=lambda p: ((mask(p) & unseen).bit_count(), rng.random()))
                        frontier = following
                        if len(parents) > 16000:
                            break
                    if destination is None:
                        break
                    extension = [destination]
                    while parents[extension[-1]] != current:
                        extension.append(parents[extension[-1]])
                    extension.reverse()
                path.extend(extension)
                for p in extension:
                    unseen &= ~mask(p)
            if unseen:
                continue
            # Every removed state must have all its coverage retained elsewhere.
            counts = {}
            for p in path:
                m = mask(p)
                while m:
                    b = m & -m
                    counts[b] = counts.get(b, 0) + 1
                    m -= b
            def removable(p):
                m = mask(p)
                while m:
                    b = m & -m
                    if counts[b] < 2:
                        return False
                    m -= b
                return True
            def remove(p):
                m = mask(p)
                while m:
                    b = m & -m
                    counts[b] -= 1
                    m -= b
            while len(path) > 1 and removable(path[0]):
                remove(path.pop(0))
            while len(path) > 1 and removable(path[-1]):
                remove(path.pop())
            i = 1
            while i < len(path) - 1:
                if removable(path[i]) and max(abs(a-b) for a,b in zip(path[i-1],path[i+1])) <= 1:
                    remove(path.pop(i))
                    i = max(1, i - 1)
                else:
                    i += 1
            if rotations:
                saved_path = path[:]
                saved_score = required * len(path) + min(max(map(abs, path[0])), max(map(abs, path[-1])))
                for _ in range(rotations):
                    if rng.randrange(2):
                        path.reverse()
                    while len(path) > 1 and removable(path[0]):
                        remove(path.pop(0))
                    while len(path) > 1 and removable(path[-1]):
                        remove(path.pop())
                    i = 1
                    while i < len(path) - 1:
                        if removable(path[i]) and max(abs(a-b) for a,b in zip(path[i-1],path[i+1])) <= 1:
                            remove(path.pop(i))
                            i = max(1, i - 1)
                        else:
                            i += 1
                    value = required * len(path) + min(max(map(abs, path[0])), max(map(abs, path[-1])))
                    if value < saved_score:
                        saved_path, saved_score = path[:], value
                    pivots = [i for i in range(len(path) - 2)
                              if max(abs(a-b) for a,b in zip(path[i],path[-1])) <= 1]
                    if pivots:
                        pivot = rng.choice(pivots)
                        path[pivot+1:] = reversed(path[pivot+1:])
                path = saved_path
            if max(map(abs, path[-1])) < max(map(abs, path[0])):
                path.reverse()
            score = required * len(path) + max(map(abs, path[0]))
            if score >= best_score:
                continue
            emitters = (0, 1, 2) + tuple(a + 3 for a in extra_axes)
            positions = [p[:3] + tuple(-v for v in p[3:]) for p in path]
            from level6 import initial_moves, move_command
            program = initial_moves(emitters, positions[0])
            for rep in range(required):
                if rep:
                    positions.reverse()
                    program.append(tuple('------'))
                for old, new in zip(positions, positions[1:]):
                    ops = ['-'] * 6
                    for e, a, b in zip(emitters, old, new):
                        ops[e] = move_command(b - a)
                    program.append(tuple(ops))
            from level5 import simulate
            if simulate(radius, board, program) == required:
                best, best_score = program, len(program)
    return best
