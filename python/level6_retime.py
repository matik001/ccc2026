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


def compress_program(radius, board, program, required):
    """Merge command spans with the same final machine state and delivery count."""
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
    program = list(program)

    def count(skip_start=-1, skip_end=-1):
        matter = submitted = 0
        for index, (create, destroy) in enumerate(exposures):
            if skip_start <= index < skip_end:
                continue
            matter = (matter | create) & ~destroy
            if matter & target == target and not matter & halo:
                submitted += 1
                matter &= ~target
        return submitted

    if count() != required:
        return program
    index = 0
    while index < len(program):
        if all(op == '-' for op in program[index]) and count(index, index + 1) == required:
            del program[index]
            del exposures[index]
            index = max(0, index - 6)
            continue
        movements, toggles, candidates = [0] * 6, [0] * 6, []
        for end in range(index, min(len(program), index + 7)):
            for emitter, operation in enumerate(program[end]):
                if operation == 'F':
                    movements[emitter] += 1
                elif operation == 'B':
                    movements[emitter] -= 1
                elif operation == 'T':
                    toggles[emitter] ^= 1
            if end == index:
                continue
            if all(abs(d) <= 1 and (not d or not t) for d, t in zip(movements, toggles)):
                operations = tuple('T' if t else 'F' if d > 0 else 'B' if d < 0 else '-'
                                   for d, t in zip(movements, toggles))
                candidates.append((end, operations))
        for end, operations in reversed(candidates):
            if count(index, end) == required:
                program[index:end + 1] = [operations]
                exposures[index:end + 1] = [exposures[end]]
                index = max(0, index - 6)
                break
        else:
            index += 1
    return program
