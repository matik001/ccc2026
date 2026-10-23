"""Trimmed full-line raster candidates for level six.

Two opposing beams create a full line. Up to four transverse beams erase
forbidden boundary cells; matter farther from the requested shape is harmless.
All retained configurations and transit ticks preserve every target cell and
never create matter in its halo.
"""

from itertools import product, permutations, combinations_with_replacement

from level2 import DIRECTIONS
from level4 import border_positions


def move_command(delta):
    return 'F' if delta > 0 else 'B' if delta < 0 else '-'


def source_offsets(radius, cells):
    rail = {point: index for index, point in enumerate(border_positions(radius))}
    result = []
    for row, col in cells:
        offsets = []
        for emitter, (dr, dc) in enumerate(DIRECTIONS.values()):
            r, c = row, col
            while (r, c) not in rail:
                r, c = r - dr, c - dc
            delta = (rail[r, c] - emitter * radius) % (6 * radius)
            offsets.append(delta if delta <= 3 * radius else delta - 6 * radius)
        result.append(tuple(offsets))
    return result


def generate_candidate(radius, board, required, limit=None, fixed_only=False):
    cells = [(r, c) for r, row in enumerate(board)
             for c, value in enumerate(row) if value in 'OM']
    target = {cell for cell in cells if board[cell[0]][cell[1]] == 'M'}
    if fixed_only and (len(target) > 16 or radius > 8):
        return None
    neighbours = {(r + dr, c + dc) for r, c in target
                  for dr, dc in DIRECTIONS.values()} - target
    sources = dict(zip(cells, source_offsets(radius, cells)))
    index = {cell: i for i, cell in enumerate(cells)}
    target_mask = sum(1 << index[cell] for cell in target)
    forbidden = sum(1 << index[cell] for cell in neighbours if cell in index)
    rays = [{offset: sum(1 << index[cell] for cell in cells
                         if sources[cell][emitter] == offset)
             for offset in range(1 - radius, radius)} for emitter in range(6)]
    cache = {}
    created = {}

    def fixed_candidate():
        if len(target) > 16 or radius > 8:
            return None
        target_sources = [sources[cell][:3] for cell in target]
        halo_sources = [sources[cell][:3] for cell in neighbours if cell in index]
        offsets = list(range(1 - radius, radius))
        values = [()] + [(x,) for x in offsets] + list(combinations_with_replacement(offsets, 2))
        options = [[(value, tuple(value.count(source[axis]) for source in target_sources))
                    for value in values] for axis in range(2)]
        lower_bound = required + max(min(map(abs, source)) for source in target_sources)
        fixed = None
        for a, ca in options[0]:
            for b, cb in options[1]:
                forced = {}
                valid = True
                for source, na, nb in zip(target_sources, ca, cb):
                    need = 2 - na - nb
                    line = source[2]
                    if need < 0 or (line in forced and forced[line] != need):
                        valid = False
                        break
                    forced[line] = need
                if not valid or sum(forced.values()) > 2:
                    continue
                allowed = {offset: {0, 1, 2} for offset in offsets}
                for x, y, z in halo_sources:
                    allowed[z].discard(2 - a.count(x) - b.count(y))
                c = []
                for offset in offsets:
                    permitted = allowed[offset]
                    count = forced.get(offset)
                    if count is None:
                        count = min(permitted, default=3)
                    if count not in permitted:
                        valid = False
                        break
                    c.extend([offset] * count)
                    if len(c) > 2:
                        valid = False
                        break
                if not valid:
                    continue
                positions, active = [0] * 6, 0
                for axis, lines in enumerate((a, b, c)):
                    for emitter, line in zip((axis, axis + 3), lines):
                        positions[emitter] = line if emitter < 3 else -line
                        active |= 1 << emitter
                setup = max(map(abs, positions))
                score = setup + required
                if (fixed is not None and score >= len(fixed)) or (limit is not None and score >= limit):
                    continue
                fixed = [tuple(move_command(d) if step < abs(d) else '-' for d in positions)
                         for step in range(setup)]
                fixed.append(tuple('T' if active & (1 << i) else '-' for i in range(6)))
                fixed.extend([tuple('------')] * (required - 1))
                if score == lower_bound:
                    return fixed
        return fixed

    def safe(state):
        if state in cache:
            return cache[state]
        positions, active = state
        one = two = three = 0
        for emitter in range(6):
            if not active & (1 << emitter):
                continue
            ray = rays[emitter].get(positions[emitter], 0)
            three, two, one = (three | (two & ray),
                               (two & ~ray) | (one & ray),
                               (one & ~ray) | (ray & ~(one | two | three)))
        result = not (two & forbidden or three & target_mask)
        cache[state] = result
        created[state] = two & target_mask
        return result

    def configs(axis, line):
        cross = [i for i in range(3) if i != axis]
        bad = [cell for cell in cells if sources[cell][axis] == line
               and (1 << index[cell]) & forbidden]
        if len(bad) > 4:
            return []
        result = set()
        for assignment in product((0, 1), repeat=len(bad)):
            groups = [[sources[cell][cross[j]] for cell, side in zip(bad, assignment)
                       if side == j] for j in range(2)]
            if any(len(group) > 2 for group in groups):
                continue
            orders = []
            for group in groups:
                if not group:
                    orders.append([(None, None)])
                elif len(group) == 1:
                    orders.append([(group[0], None), (None, group[0])])
                else:
                    orders.append(list(permutations(group)))
            for pairs in product(*orders):
                pos = [0] * 6
                pos[axis], pos[axis + 3] = line, -line
                active = (1 << axis) | (1 << (axis + 3))
                for cross_axis, pair in zip(cross, pairs):
                    for emitter, value in zip((cross_axis, cross_axis + 3), pair):
                        if value is not None:
                            pos[emitter] = value if emitter < 3 else -value
                            active |= 1 << emitter
                state = tuple(pos), active
                if safe(state):
                    result.add(state)
        return sorted(result)

    transitions = {}

    def transition(before, after):
        key = before, after
        if key in transitions:
            return transitions[key]
        old, active = before
        new, final_active = after
        delta = [b - a for a, b in zip(old, new)]
        # A guaranteed-safe fallback: deactivate, move in parallel, activate.
        fallback = []
        if active:
            fallback.append(tuple('T' if active & (1 << i) else '-' for i in range(6)))
        for step in range(max(map(abs, delta))):
            fallback.append(tuple(move_command(d) if step < abs(d) else '-' for d in delta))
        if final_active:
            fallback.append(tuple('T' if final_active & (1 << i) else '-' for i in range(6)))
        # Moving already-active emitters often permits a one-tick raster step.
        durations = [abs(d) + bool((active ^ final_active) & (1 << i))
                     for i, d in enumerate(delta)]
        length = max(max(durations), 1)
        best = fallback
        if length <= len(best):
            for late in (False, True):
                pos, on = list(old), active
                path = []
                valid = True
                for tick in range(length):
                    instructions = ['-'] * 6
                    for emitter, d in enumerate(delta):
                        was = bool(active & (1 << emitter))
                        becomes = bool(final_active & (1 << emitter))
                        if was and not becomes and tick == 0:
                            instructions[emitter] = 'T'
                            on ^= 1 << emitter
                        elif becomes and not was and tick == length - 1:
                            instructions[emitter] = 'T'
                            on ^= 1 << emitter
                        else:
                            start = int(was and not becomes)
                            if late:
                                start = length - abs(d) - int(becomes and not was)
                            if start <= tick < start + abs(d):
                                instructions[emitter] = move_command(d)
                                pos[emitter] += 1 if d > 0 else -1
                    if not safe((tuple(pos), on)):
                        valid = False
                        break
                    path.append(tuple(instructions))
                if valid and (tuple(pos), on) == after:
                    best = path
                    break
        transitions[key] = best
        return best

    start = ((0,) * 6, 0)
    best = fixed_candidate()
    if fixed_only:
        return best
    if best is not None and len(best) == required:
        return best

    def point_configs(cell):
        result = []
        for a in range(6):
            for b in range(a + 1, 6):
                if a % 3 == b % 3:
                    continue
                pos = [0] * 6
                pos[a], pos[b] = sources[cell][a], sources[cell][b]
                result.append((tuple(pos), (1 << a) | (1 << b)))
        return result

    for axis in range(3):
        lines = sorted({sources[cell][axis] for cell in target})
        layers = [configs(axis, line) for line in lines]
        if all(layers):
            orders = (layers, layers[::-1])
        else:
            # Irregular rows can be traced point by point while the remaining
            # rows are still painted in a single configuration.
            orders = []
            by_line = {line: sorted((cell for cell in target
                                    if sources[cell][axis] == line),
                                   key=lambda cell: sources[cell][(axis + 1) % 3])
                       for line in lines}
            if not any(layers):
                continue
            for backward in (False, True):
                ordered = list(zip(lines, layers))
                if backward:
                    ordered.reverse()
                for flip in (0, 1):
                    direction = []
                    for row_index, (line, layer) in enumerate(ordered):
                        if layer:
                            direction.append(layer)
                        else:
                            row_cells = by_line[line]
                            if (row_index + flip) % 2:
                                row_cells = row_cells[::-1]
                            direction.extend(point_configs(cell) for cell in row_cells)
                    if limit is None or required * len(direction) < limit:
                        orders.append(direction)
        for direction in orders:
            # Optimize startup plus all visits, retaining the chosen raster.
            costs = {state: (len(transition(start, state)), [state])
                     for state in direction[0]}
            for layer in direction[1:]:
                next_costs = {}
                for state in layer:
                    score, path = min((score + len(transition(prev, state)), path)
                                      for prev, (score, path) in costs.items())
                    next_costs[state] = score, path + [state]
                costs = next_costs
            for _, path in sorted(costs.values())[:4]:
                program = list(transition(start, path[0]))
                for before, after in zip(path, path[1:]):
                    program.extend(transition(before, after))
                for repetition in range(1, required):
                    program.append(tuple('------'))
                    path = path[::-1]
                    for before, after in zip(path, path[1:]):
                        program.extend(transition(before, after))
                if best is None or len(program) < len(best):
                    best = program
    if best is not None:
        # Transverse intersections may finish a pattern ahead of the last row.
        # Stop at the exact requested collection tick, including repetitions.
        positions, active, matter, count = [0] * 6, 0, 0, 0
        for step, instructions in enumerate(best):
            for emitter, instruction in enumerate(instructions):
                if instruction == 'F':
                    positions[emitter] += 1
                elif instruction == 'B':
                    positions[emitter] -= 1
                elif instruction == 'T':
                    active ^= 1 << emitter
            state = tuple(positions), active
            if state not in created:
                safe(state)
            matter |= created[state]
            if matter == target_mask:
                count += 1
                matter = 0
                if count == required:
                    best = best[:step + 1]
                    break
        if count == required and (limit is None or len(best) < limit):
            return best
    return None


if __name__ == '__main__':
    import json
    import pathlib
    import time
    from level6 import parse_patterns
    from level5 import simulate
    begin = time.perf_counter()
    results = {}
    for source in sorted(pathlib.Path(__file__).resolve().parent.parent.joinpath('level-6').glob('in*.txt')):
        baseline = pathlib.Path(__file__).resolve().parent.joinpath('outputs', source.name.replace('in_', 'out_')).read_text().strip().split('\n\n')
        saved = improved = valid = 0
        results[source.name] = []
        for i, (radius, board, required) in enumerate(parse_patterns(source.read_text())):
            old = len(baseline[i].splitlines())
            candidate = generate_candidate(radius, board, required, old)
            results[source.name].append(candidate)
            if candidate is not None:
                actual = simulate(radius, board, candidate)
                assert actual == required, (source.name, i, actual, required)
                saved += old - len(candidate)
                improved += 1
                print(source.name, i, old, len(candidate), flush=True)
            valid += 1
        print('TOTAL', source.name, improved, saved, 'elapsed', round(time.perf_counter() - begin, 1), flush=True)
        pathlib.Path(__file__).resolve().parent.parent.joinpath('tmp', 'static-results.json').write_text(json.dumps(results))
