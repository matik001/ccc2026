"""Shorten setup and remove stationary exposures that do not affect deliveries."""


def shorten_startup(radius, board, program):
    if not program:
        return program
    length = 6 * radius
    positions = [e * radius for e in range(6)]
    active = [False] * 6
    first = [None] * 6
    for tick, operations in enumerate(program):
        for e, operation in enumerate(operations):
            if operation == 'T':
                active[e] = not active[e]
            elif operation in 'FB':
                positions[e] = (positions[e] + (1 if operation == 'F' else -1)) % length
            offset = (positions[e] - e * radius) % length
            if offset > length // 2:
                offset -= length
            if first[e] is None and active[e] and abs(offset) < radius:
                first[e] = tick, offset
    occupied = [record for record in first if record is not None]
    if not occupied:
        return program
    earliest = min(tick for tick, _ in occupied)
    setup = max(0, max(abs(offset) - (tick - earliest) for tick, offset in occupied))
    removed = earliest - setup
    if removed <= 0:
        return program
    result = []
    for tick in range(len(program) - removed):
        operations = ['-'] * 6
        for e, record in enumerate(first):
            if record is None:
                continue
            old_first, offset = record
            activation = old_first - removed
            if tick < activation:
                if tick < abs(offset):
                    operations[e] = 'F' if offset > 0 else 'B'
            elif tick == activation:
                operations[e] = 'T'
            else:
                operations[e] = program[tick + removed][e]
        result.append(tuple(operations))
    from level5 import simulate
    if simulate(radius, board, result) == simulate(radius, board, program):
        return result
    return program


def remove_idle_ticks(radius, board, program, required):
    if not any(all(op == '-' for op in tick) for tick in program):
        return program
    from level6_multibeam import _geometry
    rays, target, halo = _geometry(radius, board)
    perimeter = radius * 6
    positions = [0] * 6
    active = [False] * 6
    exposures = []
    for tick in program:
        for emitter, operation in enumerate(tick):
            if operation == 'T':
                active[emitter] = not active[emitter]
            elif operation in 'FB':
                positions[emitter] = (positions[emitter] + (1 if operation == 'F' else -1)) % perimeter
        one = two = three = 0
        for emitter, enabled in enumerate(active):
            if not enabled:
                continue
            offset = positions[emitter]
            if offset > perimeter // 2:
                offset -= perimeter
            if emitter >= 3:
                offset = -offset
            ray = rays[emitter % 3].get(offset, 0)
            three |= two & ray
            two |= one & ray
            one |= ray
        exposures.append((two & ~three, three))
    removed = set()

    def count():
        matter = submitted = 0
        for index, (create, destroy) in enumerate(exposures):
            if index in removed:
                continue
            matter = (matter | create) & ~destroy
            if matter & target == target and not matter & halo:
                submitted += 1
                matter &= ~target
        return submitted

    if count() != required:
        return program
    for index, tick in enumerate(program):
        if all(op == '-' for op in tick):
            removed.add(index)
            if count() != required:
                removed.remove(index)
    return [tick for index, tick in enumerate(program) if index not in removed]
