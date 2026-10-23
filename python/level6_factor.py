"""Factor a six-beam exposure into three pairs of parallel lattice lines.

The two beams of an axis are interchangeable optically, so their line
coordinates are stored in sorted order.  Given the first two axes, forbidden
cells force or exclude third-axis coordinates, avoiding a Cartesian cube of
all three line-pair sets.
"""

from itertools import combinations_with_replacement, combinations
from random import Random

from level6_multibeam import _geometry


def iter_safe_configurations(radius, board, min_gain=0):
    """Yield (six physical emitter offsets, created target mask).

    All six emitters are active. Coordinates +-radius are empty rays. Duplicate
    interior coordinates on one axis create a complete line. Every emitted
    configuration preserves target cells and creates no matter in their halo.
    """
    rays, target, halo = _geometry(radius, board)
    coordinates = tuple(range(-radius, radius + 1))
    ray_lists = [tuple(axis[x] for x in coordinates) for axis in rays]
    axis_options = []
    for axis in range(2):
        entries = []
        for lo, hi in combinations_with_replacement(range(len(coordinates)), 2):
            if lo == hi:
                one, two = 0, ray_lists[axis][lo]
            else:
                one, two = ray_lists[axis][lo] | ray_lists[axis][hi], 0
            entries.append((coordinates[lo], coordinates[hi], one, two))
        axis_options.append(entries)
    c_rays = ray_lists[2]
    projection_cache = {}

    def project(mask):
        if mask not in projection_cache:
            value = 0
            for i, ray in enumerate(c_rays):
                if mask & ray:
                    value |= 1 << i
            projection_cache[mask] = value
        return projection_cache[mask]

    all_coordinates = (1 << len(coordinates)) - 1
    for alo, ahi, a1, a2 in axis_options[0]:
        a = a1 | a2
        for blo, bhi, b1, b2 in axis_options[1]:
            b = b1 | b2
            over = (a2 & b) | (b2 & a)
            if over & target:
                continue
            two = (a2 & ~b) | (b2 & ~a) | (a1 & b1)
            one = (a1 & ~b) | (b1 & ~a)
            required = project(two & halo)
            if required.bit_count() > 2:
                continue

            # Two different third-axis coordinates, each with unit intensity.
            banned = project((two & target) | (one & halo))
            if not required & banned:
                allowed = all_coordinates ^ banned
                missing = required.bit_count()
                if missing == 2:
                    choices = [tuple(i for i in range(len(coordinates)) if required & (1 << i))]
                elif missing == 1:
                    forced = required.bit_length() - 1
                    choices = ((min(forced, i), max(forced, i))
                               for i in range(len(coordinates))
                               if i != forced and allowed & (1 << i))
                else:
                    choices = combinations((i for i in range(len(coordinates))
                                            if allowed & (1 << i)), 2)
                for clo, chi in choices:
                    c = c_rays[clo] | c_rays[chi]
                    created = ((two & ~c) | (one & c)) & target
                    if created.bit_count() >= min_gain:
                        yield (alo, blo, coordinates[clo], -ahi, -bhi, -coordinates[chi]), created

            # Two coincident third-axis rays: intensity two along one line.
            if required.bit_count() <= 1:
                banned = project(((a | b) & target) | (halo & ~(a | b)))
                allowed = all_coordinates ^ banned
                if required:
                    allowed &= required
                while allowed:
                    bit = allowed & -allowed
                    ci = bit.bit_length() - 1
                    c = c_rays[ci]
                    created = ((two & ~c) | (c & ~(a | b))) & target
                    if created.bit_count() >= min_gain:
                        coord = coordinates[ci]
                        yield (alo, blo, coord, -ahi, -bhi, -coord), created
                    allowed -= bit


def enumerate_configs(radius, board, max_states=None, min_gain=0):
    """Return canonical states, created masks and the complete target mask.

    max_states optionally uses a deterministic reservoir to bound memory.
    Without it, the exact full graph is available to the caller.
    """
    states, masks = [], []
    rng = Random(66500 + radius)
    for i, (state, mask) in enumerate(iter_safe_configurations(radius, board, min_gain)):
        if max_states is None or len(states) < max_states:
            states.append(state)
            masks.append(mask)
        else:
            selected = rng.randrange(i + 1)
            if selected < max_states:
                states[selected], masks[selected] = state, mask
    return states, masks, _geometry(radius, board)[1]


def sample_configs(radius, board, samples=10000, top=100):
    """Find promising safe starts without allocating the complete graph.

    Sample the first two line pairs, then derive and optimize the third pair.
    Coincident pairs are deliberately oversampled because full-line exposures
    can paint substantially more cells than six isolated crossings.
    """
    rays, target, halo = _geometry(radius, board)
    coordinates = tuple(range(-radius, radius + 1))
    ray_lists = [tuple(axis[x] for x in coordinates) for axis in rays]
    entries = []
    duplicated = []
    for axis in range(2):
        options, doubles = [], []
        for lo, hi in combinations_with_replacement(range(len(coordinates)), 2):
            if lo == hi:
                one, two = 0, ray_lists[axis][lo]
            else:
                one, two = ray_lists[axis][lo] | ray_lists[axis][hi], 0
            item = coordinates[lo], coordinates[hi], one, two
            options.append(item)
            if lo == hi:
                doubles.append(item)
        entries.append(options)
        duplicated.append(doubles)
    c_rays = ray_lists[2]
    rng = Random(665021 + radius)
    projections = {}

    def project(mask):
        if mask not in projections:
            projections[mask] = sum(1 << i for i, ray in enumerate(c_rays) if mask & ray)
        return projections[mask]

    kept = {}

    def consider(alo, ahi, blo, bhi, clo, chi, created):
        if not created:
            return
        state = (alo, blo, coordinates[clo], -ahi, -bhi, -coordinates[chi])
        setup = max(map(abs, state))
        rank = (setup, sum(map(abs, state)), state)
        previous = kept.get(created)
        if previous is None or rank < previous[0]:
            kept[created] = rank, state

    all_coords = (1 << len(coordinates)) - 1
    for trial in range(samples):
        alo, ahi, a1, a2 = rng.choice(duplicated[0] if trial % 4 == 0 else entries[0])
        blo, bhi, b1, b2 = rng.choice(duplicated[1] if trial % 4 == 1 else entries[1])
        a, b = a1 | a2, b1 | b2
        over = (a2 & b) | (b2 & a)
        if over & target:
            continue
        two = (a2 & ~b) | (b2 & ~a) | (a1 & b1)
        one = (a1 & ~b) | (b1 & ~a)
        required = project(two & halo)
        if required.bit_count() > 2:
            continue
        banned = project((two & target) | (one & halo))
        if not required & banned:
            allowed = [i for i in range(len(coordinates)) if not banned & (1 << i)]
            forced = [i for i in allowed if required & (1 << i)]
            gain = {i: (one & c_rays[i] & target).bit_count() for i in allowed}
            preferred = sorted((i for i in allowed if i not in forced),
                               key=lambda i: (-gain[i], abs(coordinates[i])))[:6]
            if len(forced) == 2:
                choices = [forced]
            elif len(forced) == 1:
                choices = [(forced[0], i) for i in preferred]
            else:
                choices = combinations(preferred, 2)
            for clo, chi in choices:
                clo, chi = min(clo, chi), max(clo, chi)
                c = c_rays[clo] | c_rays[chi]
                created = ((two & ~c) | (one & c)) & target
                consider(alo, ahi, blo, bhi, clo, chi, created)
        if required.bit_count() <= 1:
            banned = project(((a | b) & target) | (halo & ~(a | b)))
            allowed = all_coords ^ banned
            if required:
                allowed &= required
            while allowed:
                bit = allowed & -allowed
                ci = bit.bit_length() - 1
                c = c_rays[ci]
                created = ((two & ~c) | (c & ~(a | b))) & target
                consider(alo, ahi, blo, bhi, ci, ci, created)
                allowed -= bit
    ranked = sorted(kept, key=lambda mask: (-mask.bit_count(), kept[mask][0]))
    selected = ranked[:top]
    # Preserve starts that cover spatially different cells, even if their
    # first exposure is less dense than the strongest configuration.
    if top >= 12:
        selected = ranked[:3 * top // 4]
        unseen = target
        for mask in selected:
            unseen &= ~mask
        for _ in range(top - len(selected)):
            if not ranked:
                break
            mask = max(ranked, key=lambda m: ((m & unseen).bit_count(),
                                             m.bit_count(), -kept[m][0][0]))
            if mask in selected:
                break
            selected.append(mask)
            unseen &= ~mask
        selected_set = set(selected)
        selected.extend(mask for mask in ranked if mask not in selected_set)
        selected = selected[:top]
    return [kept[mask][1] for mask in selected], selected, target


if __name__ == '__main__':
    import argparse
    import pathlib
    import time
    from level6 import parse_patterns
    parser = argparse.ArgumentParser()
    parser.add_argument('--file', default='in_level-6_1-small.txt')
    parser.add_argument('--indices', default='0,1,2')
    parser.add_argument('--min-gain', type=int, default=0)
    args = parser.parse_args()
    root = pathlib.Path(__file__).resolve().parent.parent
    cases = parse_patterns(root.joinpath('level-6', args.file).read_text())
    for index in map(int, args.indices.split(',')):
        radius, board, required = cases[index]
        start = time.perf_counter()
        count = maximum = useful = 0
        seen = 0
        for state, mask in iter_safe_configurations(radius, board, args.min_gain):
            count += 1
            useful += bool(mask)
            maximum = max(maximum, mask.bit_count())
            seen |= mask
        target = _geometry(radius, board)[1]
        print(index, 'radius', radius, 'cells', target.bit_count(), 'configs', count,
              'useful', useful, 'maxgain', maximum, 'complete', seen == target,
              'seconds', round(time.perf_counter() - start, 3), flush=True)
