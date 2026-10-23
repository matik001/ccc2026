"""Short six-beam paths and cycles that repair temporary excess matter."""

from itertools import product, combinations_with_replacement
from time import monotonic

from level6_multibeam import _geometry


_RAW_CONFIGS = {}
_RAW_LOOKUPS = {}


def _raw_configs(radius, board):
    """All canonical exposures, including temporary unwanted matter."""
    if radius in _RAW_CONFIGS:
        return _RAW_CONFIGS[radius]
    rays, _, _ = _geometry(radius, board)
    pairs = [[(a, b, lines[a] | lines[b], lines[a] & lines[b])
              for a, b in combinations_with_replacement(range(-radius, radius + 1), 2)]
             for lines in rays]
    result = []
    for a0, b0, one0, two0 in pairs[0]:
        for a1, b1, one1, two1 in pairs[1]:
            one, two = one0 | one1, two0 | two1 | (one0 & one1)
            three = (two0 & one1) | (one0 & two1)
            for a2, b2, one2, two2 in pairs[2]:
                erased = three | (two & one2) | (one & two2)
                made = (two | two2 | (one & one2)) & ~erased
                state = a0, a1, a2, -b0, -b1, -b2
                result.append((state, made, erased))
    result.sort(key=lambda entry: max(map(abs, entry[0])))
    _RAW_CONFIGS[radius] = result
    return result


def generate_two_phase_candidate(radius, board, required, budget=None,
                                 max_radius=5, seconds=2):
    """Create excess matter, then erase it; repeat a two-exposure cycle."""
    if radius > max_radius:
        return None
    deadline = monotonic() + seconds
    _, target, halo = _geometry(radius, board)
    raw = _raw_configs(radius, board)
    by_bit = {}
    for state, made, erased in raw:
        if made & halo or erased & target:
            continue
        effective = (made & target) | (erased & halo)
        bits = effective
        while bits:
            bit = bits & -bits
            by_bit.setdefault(bit, []).append((state, effective))
            bits -= bit
    best = None
    best_score = budget if budget is not None else float('inf')
    from level6 import initial_moves, move_command
    for state, made, _ in raw:
        setup = max(map(abs, state))
        if required * 2 + setup >= best_score:
            continue
        needed = (target & ~made) | (halo & made)
        if not needed:
            program = initial_moves(tuple(range(6)), state)
            program.extend([tuple('------')] * (required - 1))
            if len(program) < best_score:
                best, best_score = program, len(program)
            continue
        bits = []
        remaining = needed
        while remaining:
            bit = remaining & -remaining
            if bit not in by_bit:
                break
            bits.append(bit)
            remaining -= bit
        if remaining:
            continue
        rarest = min(bits, key=lambda b: len(by_bit[b]))
        for other, effective in by_bit[rarest]:
            if effective & needed != needed:
                continue
            if not all(abs(a - b) <= 1 for a, b in zip(state, other)):
                continue
            program = initial_moves(tuple(range(6)), state)
            forward = tuple(move_command(b - a) for a, b in zip(state, other))
            backward = tuple(move_command(a - b) for a, b in zip(state, other))
            program.append(forward)
            for repetition in range(1, required):
                program.extend((backward, forward))
            best, best_score = program, len(program)
            break
        if monotonic() > deadline:
            break
    return best


def generate_three_phase_candidate(radius, board, required, budget=None,
                                   max_radius=5, seconds=1.5):
    """Bounded local search for a productive destructive three-state cycle."""
    if radius > max_radius or (budget is not None and budget <= 3 * required):
        return None
    deadline = monotonic() + seconds
    _, target, halo = _geometry(radius, board)
    raw = _raw_configs(radius, board)
    if radius not in _RAW_LOOKUPS:
        _RAW_LOOKUPS[radius] = {state: (made, erase) for state, made, erase in raw}
    lookup = _RAW_LOOKUPS[radius]
    moves = tuple(product((-1, 0, 1), repeat=6))
    neighbours = {}

    def nearby(state):
        if state not in neighbours:
            found = []
            for delta in moves:
                other = tuple(a + b for a, b in zip(state, delta))
                effect = lookup.get(other)
                if effect is not None:
                    found.append((other, *effect))
            neighbours[state] = found
        return neighbours[state]

    endings = [(state, made, erase) for state, made, erase in raw
               if not (made & halo or erase & target)]
    endings.sort(key=lambda entry: (-(entry[1] & target).bit_count(),
                                    -(entry[2] & halo).bit_count(), max(map(abs, entry[0]))))
    # Several configurations often have identical effects. Retain a small
    # selection of physical positions for each effect before exploring more.
    counts, selected = {}, []
    for entry in endings:
        key = entry[1] & target, entry[2] & halo
        count = counts.get(key, 0)
        counts[key] = count + 1
        if count < 3:
            selected.append(entry)
    best = None
    best_score = budget if budget is not None else float('inf')
    from level6 import initial_moves, move_command
    for end, endmade, enderase in selected[:160]:
        if monotonic() > deadline:
            break
        middles = []
        for middle, midmade, miderase in nearby(end):
            if midmade & halo & ~enderase or miderase & target & ~endmade:
                continue
            missing = target & ~(midmade | endmade)
            cleared = (miderase | enderase) & halo
            middles.append((missing.bit_count(), -cleared.bit_count(),
                            middle, missing, cleared))
        middles.sort()
        for _, _, middle, missing, cleared in middles[:12]:
            for start, made, _ in nearby(middle):
                if made & missing != missing or made & halo & ~cleared:
                    continue
                cycle = all(abs(a - b) <= 1 for a, b in zip(start, end))
                length = (3 if cycle else 4) * required - (0 if cycle else 1)
                score = length + max(map(abs, start))
                if score >= best_score:
                    continue
                path = [start, middle, end]
                for repetition in range(1, required):
                    path.extend((start, middle, end) if cycle else (middle, start, middle, end))
                program = initial_moves(tuple(range(6)), path[0])
                matter = made & (target | halo)
                count = int((matter & target) == target and not matter & halo)
                if count:
                    matter &= ~target
                if count == required:
                    best, best_score = program, len(program)
                    continue
                for before, after in zip(path, path[1:]):
                    program.append(tuple(move_command(b - a) for a, b in zip(before, after)))
                    create, erase = lookup[after]
                    matter = ((matter & ~erase) | create) & (target | halo)
                    if matter & target == target and not matter & halo:
                        count += 1
                        matter &= ~target
                        if count == required:
                            break
                if count == required and len(program) < best_score:
                    best, best_score = program, len(program)
    return best


def _six_program(path, required):
    from level6 import initial_moves, move_command
    if max(map(abs, path[-1])) < max(map(abs, path[0])):
        path = path[::-1]
    positions = [state[:3] + tuple(-v for v in state[3:]) for state in path]
    program = initial_moves(tuple(range(6)), positions[0])
    for repetition in range(required):
        if repetition:
            positions.reverse()
            program.append(tuple('------'))
        for before, after in zip(positions, positions[1:]):
            program.append(tuple(move_command(b - a) for a, b in zip(before, after)))
    return program


def generate_short_candidate(radius, board, required, budget=None, max_radius=5,
                             max_cells=24, seconds=4):
    """Exhaust all six-beam states and seek a one-, two-, or three-tick cover."""
    if radius > max_radius or sum(row.count('M') for row in board) > max_cells:
        return None
    deadline = monotonic() + seconds
    from level6_factor import enumerate_configs
    states, masks, target = enumerate_configs(radius, board, min_gain=1)
    safe = [(state[:3] + tuple(-v for v in state[3:]), mask)
            for state, mask in zip(states, masks)]
    safe.sort(key=lambda entry: (max(map(abs, entry[0])), -entry[1].bit_count()))
    best = None
    best_score = budget if budget is not None else float('inf')
    by_bit = {}
    by_mask = {}
    for state, mask in safe:
        by_mask.setdefault(mask, []).append(state)
        if mask == target:
            score = required + max(map(abs, state))
            if score < best_score:
                best, best_score = _six_program([state], required), score
        bits = mask
        while bits:
            bit = bits & -bits
            by_bit.setdefault(bit, []).append((state, mask))
            bits -= bit
    if best is not None:
        return best
    # Candidate second states must cover every missing bit. Search the rarest
    # missing bit first to keep the exact two-state search compact.
    union = 0
    for mask in by_mask:
        union |= mask
    if union != target:
        return None
    max_gain = max((mask.bit_count() for mask in by_mask), default=0)
    for state, mask in safe:
        setup = max(map(abs, state))
        if required * 2 + setup >= best_score:
            continue
        missing = target ^ mask
        if missing.bit_count() > max_gain:
            continue
        bits = []
        remaining = missing
        while remaining:
            bit = remaining & -remaining
            bits.append(bit)
            remaining -= bit
        rarest = min(bits, key=lambda b: len(by_bit[b]))
        for other, made in by_bit[rarest]:
            if made & missing != missing:
                continue
            if all(abs(a - b) <= 1 for a, b in zip(state, other)):
                best, best_score = _six_program([state, other], required), required * 2 + setup
                break
        if monotonic() > deadline:
            return best
    if best is not None:
        return best
    # A three-state cover has two endpoints adjacent to a central state.
    # Keep the search bounded; the two-state search above was exhaustive.
    lookup = {state: mask for state, mask in safe}
    changes = list(product((-1, 0, 1), repeat=6))
    centers = sorted(safe, key=lambda entry: (-entry[1].bit_count(), max(map(abs, entry[0]))))
    for state, made in centers[:400]:
        if monotonic() > deadline:
            break
        missing = target ^ made
        neighbours = []
        for change in changes:
            other = tuple(a + b for a, b in zip(state, change))
            mask = lookup.get(other, 0)
            if mask & missing:
                neighbours.append((other, mask))
        neighbours.sort(key=lambda entry: (-(entry[1] & missing).bit_count(), max(map(abs, entry[0]))))
        for left, leftmask in neighbours[:60]:
            rest = missing & ~leftmask
            for right, rightmask in neighbours:
                if rightmask & rest != rest:
                    continue
                setup = min(max(map(abs, left)), max(map(abs, right)))
                score = required * 3 + setup
                if score < best_score:
                    best, best_score = _six_program([left, state, right], required), score
                break
    return best
