"""Plan exposures backwards, allowing mistakes repaired by later exposures."""

from itertools import product
from random import Random
from heapq import heappush, heappop

from level6_multibeam import _geometry


class ReverseGraph:
    def __init__(self, radius, board):
        self.radius = radius
        self.rays, self.target, self.halo = _geometry(radius, board)
        self.effects = {}
        self.edges = {}
        self.choices = {x: tuple(range(max(-radius, x-1), min(radius, x+1)+1))
                        for x in range(-radius, radius+1)}
        self.cell_sources = []
        pending = self.target | self.halo
        while pending:
            bit = pending & -pending
            self.cell_sources.append((bit, tuple(next(k for k,v in axis.items() if v & bit)
                                                for axis in self.rays)))
            pending -= bit

    def effect(self, state):
        if state in self.effects:
            return self.effects[state]
        one = two = three = 0
        for axis, value in enumerate(state):
            ray = self.rays[axis % 3][value]
            three |= two & ray
            two |= one & ray
            one |= ray
        value = (two & ~three) & (self.target | self.halo), three & (self.target | self.halo)
        self.effects[state] = value
        return value

    def neighbours(self, state):
        if state not in self.edges:
            self.edges[state] = [n for n in product(*(self.choices[x] for x in state))
                                 if n != state and all(n[a] <= n[a+3] for a in range(3))]
        return self.edges[state]


def generate_candidate(radius, board, required, attempts=8, budget=None):
    from level6_factor import sample_configs
    from level6 import initial_moves, move_command
    from level5 import simulate
    graph = ReverseGraph(radius, board)
    samples, _, _ = sample_configs(radius, board, samples=2500, top=80)
    starts = [p[:3] + tuple(-v for v in p[3:]) for p in samples]
    if not starts:
        return None
    rng = Random(68721 + graph.target)
    best, bound = None, budget or 1000000

    def valid(state, need_m, need_o):
        made, erased = graph.effect(state)
        return not (made & need_o or erased & need_m)

    # Final cleanup exposures need not create any target cells. They can be
    # valuable because they permit earlier exposures to paint entire lines.
    start_effects = {graph.effect(p): p for p in starts}
    for sample in range(5000):
        pairs = []
        for axis in range(3):
            a, b = rng.randint(-radius, radius), rng.randint(-radius, radius)
            if sample % 4 == axis:
                b = a
            pairs.append(sorted((a, b)))
        state = tuple(pair[0] for pair in pairs) + tuple(pair[1] for pair in pairs)
        made, erased = graph.effect(state)
        if not (made & graph.halo or erased & graph.target) and (made or erased):
            key = made & graph.target, erased & graph.halo
            previous = start_effects.get(key)
            if previous is None or max(map(abs, state)) < max(map(abs, previous)):
                start_effects[key] = state
    starts = list(start_effects.values())

    for trial in range(attempts):
        halo_weight = (0.15, 0.4, 0.8, 1.3)[trial % 4]
        ranked = sorted(starts, key=lambda p: (
            -(graph.effect(p)[0] & graph.target).bit_count()
            - halo_weight * (graph.effect(p)[1] & graph.halo).bit_count(),
            max(map(abs, p))))
        start = ranked[0] if trial < 4 else rng.choice(ranked[:min(40, len(ranked))])
        path = [start]
        made, erased = graph.effect(start)
        need_m, need_o = graph.target & ~made, graph.halo & ~erased
        while need_m and len(path) * required < bound:
            if trial >= 4:
                # Erasing a halo cell is especially valuable when it unlocks
                # a long row of still-needed target cells for an earlier beam.
                line_values = [{value: (ray & need_m).bit_count()
                                / (1 + (ray & need_o).bit_count())
                                for value, ray in axis.items()} for axis in graph.rays]
                erase_values = {bit: max(line_values[axis][xyz[axis]] for axis in range(3))
                                for bit, xyz in graph.cell_sources if bit & need_o}
                interested_o = sum(bit for bit, value in erase_values.items() if value)
            else:
                erase_values = None
                interested_o = need_o
            erasure_scores = {}

            def erase_score(mask):
                mask &= need_o
                if erase_values is None:
                    return mask.bit_count()
                if mask not in erasure_scores:
                    score = 0.0
                    rest = mask
                    while rest:
                        bit = rest & -rest
                        score += erase_values.get(bit, 0.0)
                        rest -= bit
                    erasure_scores[mask] = score
                return erasure_scores[mask]

            options = [p for p in graph.neighbours(path[-1]) if valid(p, need_m, need_o)]
            useful = [p for p in options if
                      graph.effect(p)[0] & need_m or graph.effect(p)[1] & interested_o]
            if useful:
                def quick(p):
                    made, erased = graph.effect(p)
                    return (made & need_m).bit_count() + halo_weight * erase_score(erased)
                useful.sort(key=quick, reverse=True)
                short = useful[:8]
                def score(p):
                    made, erased = graph.effect(p)
                    m, o = need_m & ~made, need_o & ~erased
                    future = max(((graph.effect(q)[0] & m).bit_count()
                                  + halo_weight * erase_score(graph.effect(q)[1] & o)
                                  for q in graph.neighbours(p) if valid(q, m, o)), default=0)
                    return quick(p) + (0.45 if trial % 2 else 0.7) * future, rng.random()
                extension = [max(short, key=score)]
            else:
                # An admissible distance to the nearest still-needed cell avoids
                # flooding the increasingly permissive configuration space.
                parents = {path[-1]: None}
                distances = {path[-1]: 0}
                locations = [(p, 1 if bit & need_m else 2)
                             for bit, p in graph.cell_sources if bit & (need_m | interested_o)]
                def distance(p):
                    best_distance = radius * 2
                    for xyz, needed_index in locations:
                        distances_to_rays = sorted(abs(p[e] - xyz[e % 3]) for e in range(6))
                        best_distance = min(best_distance, distances_to_rays[needed_index])
                    return best_distance
                frontier = [(distance(path[-1]), 0, rng.random(), path[-1])]
                destination = None
                expanded = 0
                while frontier and destination is None and expanded < 600:
                    _, negative_cost, _, p = heappop(frontier)
                    cost = -negative_cost
                    if cost != distances[p]:
                        continue
                    expanded += 1
                    for q in graph.neighbours(p):
                        if distances.get(q, 10**9) <= cost + 1 or not valid(q, need_m, need_o):
                            continue
                        parents[q] = p
                        distances[q] = cost + 1
                        made, erased = graph.effect(q)
                        if made & need_m or erased & interested_o:
                            destination = q
                            break
                        heappush(frontier, (cost + 1 + distance(q), -cost - 1, rng.random(), q))
                if destination is None:
                    break
                extension = [destination]
                while parents[extension[-1]] != path[-1]:
                    extension.append(parents[extension[-1]])
                extension.reverse()
            for p in extension:
                made, erased = graph.effect(p)
                need_m &= ~made
                need_o &= ~erased
            path.extend(extension)
        if need_m:
            continue
        positions = [p[:3] + tuple(-v for v in p[3:]) for p in reversed(path)]
        program = initial_moves(tuple(range(6)), positions[0])
        for repetition in range(required):
            if repetition:
                program.append(tuple('TTTTTT'))
                deltas = tuple(b-a for a,b in zip(positions[-1], positions[0]))
                for step in range(max(map(abs, deltas))):
                    program.append(tuple(move_command(d) if step < abs(d) else '-' for d in deltas))
                program.append(tuple('TTTTTT'))
            for before, after in zip(positions, positions[1:]):
                program.append(tuple(move_command(b-a) for a,b in zip(before, after)))
        if program:
            program = program[:bound - 1]
            actual = simulate(radius, board, program)
            if actual >= required:
                # Collections can finish before the planned final exposure.
                # Counting is monotone, with at most one collection per tick.
                lo, hi = 0, len(program)
                while lo + 1 < hi:
                    middle = (lo + hi) // 2
                    if simulate(radius, board, program[:middle]) >= required:
                        hi = middle
                    else:
                        lo = middle
                best, bound = program[:hi], hi
    return best
