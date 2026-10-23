import sys


def generate_sequence(target, max_pace=5):
    """
    Generate optimal sequence to reach target position.
    Each pace value takes specific time and moves specific distance:
    - Pace |p| (1-5): takes |p| time units, moves 1 space unit (sign determines direction)
    - Pace 0: takes 1 time unit, moves 0 space units

    Valid transitions:
    0 ↔ ±5 ↔ 4 ↔ 3 ↔ 2 ↔ 1 ↔ 0
    """
    sign = 1 if target >= 0 else -1
    distance = abs(target)

    if distance == 0:
        return [0]

    def calc_time(paces):
        """Calculate total time for a sequence"""
        time = 0
        for p in paces:
            if p == 0:
                time += 1
            else:
                time += abs(p)
        return time

    best_solution = None
    best_time = float("inf")

    # Try different paths: go down to min_pace then back up
    for min_pace in range(max_pace, 0, -1):
        # Down: 5→4→3→...→min_pace
        down = list(range(max_pace, min_pace - 1, -1))
        # Up: (min_pace+1)→...→4→5
        up = list(range(min_pace + 1, max_pace + 1))

        # Calculate distance (each pace moves 1 unit of space)
        base_distance = len(down) + len(up)

        if base_distance == distance:
            sequence = [0] + down + up + [0]
            time = calc_time(sequence)
            if time < best_time:
                best_time = time
                best_solution = sequence
        elif base_distance < distance:
            # Need to maintain min_pace for extra distance
            remaining = distance - base_distance
            sequence = [0] + down + [min_pace] * remaining + up + [0]
            time = calc_time(sequence)
            if time < best_time:
                best_time = time
                best_solution = sequence

    if best_solution:
        paces = [p * sign if p != 0 else 0 for p in best_solution]
        return paces

    # Fallback: shouldn't reach here
    paces = [0] + [1 * sign] * distance + [0]
    return paces


def calculate_total_time(x_seq, y_seq):
    """
    Calculate total time for executing two sequences in parallel.
    Returns the time when both sequences complete.
    """
    time = 0
    x_idx, y_idx = 0, 0
    x_remaining, y_remaining = 0, 0

    while x_idx < len(x_seq) or y_idx < len(y_seq):
        # Start new pace on X if needed
        if x_remaining == 0 and x_idx < len(x_seq):
            x_pace = x_seq[x_idx]
            x_remaining = abs(x_pace) if x_pace != 0 else 1
            x_idx += 1

        # Start new pace on Y if needed
        if y_remaining == 0 and y_idx < len(y_seq):
            y_pace = y_seq[y_idx]
            y_remaining = abs(y_pace) if y_pace != 0 else 1
            y_idx += 1

        # Decrement time remaining
        if x_remaining > 0:
            x_remaining -= 1
        if y_remaining > 0:
            y_remaining -= 1

        time += 1

    return time


def check_collision(x_seq, y_seq, asteroid_x, asteroid_y, debug=False):
    """
    Check if path collides with asteroid.
    Ship has a 5x5 square safety area (radius 2).
    Position is maintained during entire pace duration.
    """
    pos_x, pos_y = 0, 0
    time = 0

    max_len = max(len(x_seq), len(y_seq))
    x_idx, y_idx = 0, 0
    x_remaining, y_remaining = 0, 0

    # Check initial position
    if abs(pos_x - asteroid_x) <= 2 and abs(pos_y - asteroid_y) <= 2:
        return True

    while time < 10000 and (x_idx < len(x_seq) or y_idx < len(y_seq)):
        # Start new pace on X if needed
        if x_remaining == 0 and x_idx < len(x_seq):
            x_pace = x_seq[x_idx]
            x_remaining = abs(x_pace) if x_pace != 0 else 1
            x_idx += 1

        # Start new pace on Y if needed
        if y_remaining == 0 and y_idx < len(y_seq):
            y_pace = y_seq[y_idx]
            y_remaining = abs(y_pace) if y_pace != 0 else 1
            y_idx += 1

        # Decrement time remaining
        if x_remaining > 0:
            x_remaining -= 1
            # Move happens at the END of the pace duration
            if x_remaining == 0 and x_idx > 0:
                current_pace = x_seq[x_idx - 1]
                if current_pace != 0:
                    pos_x += 1 if current_pace > 0 else -1

        if y_remaining > 0:
            y_remaining -= 1
            if y_remaining == 0 and y_idx > 0:
                current_pace = y_seq[y_idx - 1]
                if current_pace != 0:
                    pos_y += 1 if current_pace > 0 else -1

        time += 1

        # Check collision at current position
        if abs(pos_x - asteroid_x) <= 2 and abs(pos_y - asteroid_y) <= 2:
            if debug:
                print(f"Collision at time {time}: pos=({pos_x},{pos_y})")
            return True

    return False


def verify_sequence_reaches_target(seq, target):
    """Verify that a sequence reaches the target position"""
    pos = 0
    for pace in seq:
        if pace != 0:
            pos += 1 if pace > 0 else -1
    return pos == target


def find_safe_sequences(
    x_target, y_target, asteroid_x, asteroid_y, time_limit, max_pace=5
):
    """Find sequences that avoid asteroid collision and meet time limit"""
    base_x = generate_sequence(x_target, max_pace)
    base_y = generate_sequence(y_target, max_pace)

    # Check if base solution works (no collision AND within time limit)
    if not check_collision(base_x, base_y, asteroid_x, asteroid_y):
        total_time = calculate_total_time(base_x, base_y)
        if total_time <= time_limit:
            return base_x, base_y
        else:
            print(
                f"WARNING: Base solution time={total_time} exceeds limit={time_limit}",
                file=sys.stderr,
            )

    # Strategy 1: If asteroid is on direct path, use perpendicular detour
    # This is critical for cases like (0,16) with asteroid at (0,5)

    # Check if we need X detour (asteroid blocks Y path)
    if x_target == 0 and asteroid_x == 0:
        # Go sideways first, then up/down, then back
        for detour_x in [3, 5, 7, 10, 15, 20]:
            for sign_x in [1, -1]:
                actual_detour_x = detour_x * sign_x
                seq_x_out = generate_sequence(actual_detour_x, max_pace)
                seq_x_back = generate_sequence(-actual_detour_x, max_pace)

                # Try different timings
                for y_delay in range(0, min(30, time_limit)):
                    y_delayed = [0] * y_delay + base_y
                    x_detour = seq_x_out + seq_x_back
                    if not check_collision(x_detour, y_delayed, asteroid_x, asteroid_y):
                        total_time = calculate_total_time(x_detour, y_delayed)
                        if total_time <= time_limit:
                            return x_detour, y_delayed

                # Try completing detour before Y starts
                x_detour = seq_x_out + seq_x_back
                y_delayed = [0] * len(x_detour) + base_y
                if not check_collision(x_detour, y_delayed, asteroid_x, asteroid_y):
                    total_time = calculate_total_time(x_detour, y_delayed)
                    if total_time <= time_limit:
                        return x_detour, y_delayed

    # Check if we need Y detour (asteroid blocks X path)
    if y_target == 0 and asteroid_y == 0:
        for detour_y in [3, 5, 7, 10, 15, 20]:
            for sign_y in [1, -1]:
                actual_detour_y = detour_y * sign_y
                seq_y_out = generate_sequence(actual_detour_y, max_pace)
                seq_y_back = generate_sequence(-actual_detour_y, max_pace)

                for x_delay in range(0, min(30, time_limit)):
                    x_delayed = [0] * x_delay + base_x
                    y_detour = seq_y_out + seq_y_back
                    if not check_collision(x_delayed, y_detour, asteroid_x, asteroid_y):
                        total_time = calculate_total_time(x_delayed, y_detour)
                        if total_time <= time_limit:
                            return x_delayed, y_detour

                y_detour = seq_y_out + seq_y_back
                x_delayed = [0] * len(y_detour) + base_x
                if not check_collision(x_delayed, y_detour, asteroid_x, asteroid_y):
                    total_time = calculate_total_time(x_delayed, y_detour)
                    if total_time <= time_limit:
                        return x_delayed, y_detour

    # Strategy 2: Sequential movement - complete Y first, then X
    for y_delay in range(0, min(50, time_limit)):
        y_delayed = [0] * y_delay + base_y
        x_delayed = [0] * len(y_delayed) + base_x
        if not check_collision(x_delayed, y_delayed, asteroid_x, asteroid_y):
            total_time = calculate_total_time(x_delayed, y_delayed)
            if total_time <= time_limit:
                return x_delayed, y_delayed

    # Strategy 3: Sequential movement - complete X first, then Y
    for x_delay in range(0, min(50, time_limit)):
        x_delayed = [0] * x_delay + base_x
        y_delayed = [0] * len(x_delayed) + base_y
        if not check_collision(x_delayed, y_delayed, asteroid_x, asteroid_y):
            total_time = calculate_total_time(x_delayed, y_delayed)
            if total_time <= time_limit:
                return x_delayed, y_delayed

    # Strategy 4: Detour strategies - go around the asteroid
    for detour_offset in range(3, 25):
        # Try detouring in Y
        for sign_y in [1, -1]:
            detour_y = y_target + (detour_offset * sign_y)
            seq_y_detour = generate_sequence(detour_y, max_pace)
            seq_y_back = generate_sequence(y_target - detour_y, max_pace)
            y_detour = seq_y_detour + seq_y_back

            # Try with original X
            if not check_collision(base_x, y_detour, asteroid_x, asteroid_y):
                total_time = calculate_total_time(base_x, y_detour)
                if total_time <= time_limit:
                    return base_x, y_detour

            # Try with delayed X
            for x_delay in range(1, min(50, time_limit)):
                x_delayed = [0] * x_delay + base_x
                if not check_collision(x_delayed, y_detour, asteroid_x, asteroid_y):
                    total_time = calculate_total_time(x_delayed, y_detour)
                    if total_time <= time_limit:
                        return x_delayed, y_detour

        # Try detouring in X
        for sign_x in [1, -1]:
            detour_x = x_target + (detour_offset * sign_x)
            seq_x_detour = generate_sequence(detour_x, max_pace)
            seq_x_back = generate_sequence(x_target - detour_x, max_pace)
            x_detour = seq_x_detour + seq_x_back

            # Try with original Y
            if not check_collision(x_detour, base_y, asteroid_x, asteroid_y):
                total_time = calculate_total_time(x_detour, base_y)
                if total_time <= time_limit:
                    return x_detour, base_y

            # Try with delayed Y
            for y_delay in range(1, min(50, time_limit)):
                y_delayed = [0] * y_delay + base_y
                if not check_collision(x_detour, y_delayed, asteroid_x, asteroid_y):
                    total_time = calculate_total_time(x_detour, y_delayed)
                    if total_time <= time_limit:
                        return x_detour, y_delayed

    # Strategy 5: Simple delays (more extensive)
    for delay in range(1, min(300, time_limit)):
        # Delay X
        x_delayed = [0] * delay + base_x
        if not check_collision(x_delayed, base_y, asteroid_x, asteroid_y):
            total_time = calculate_total_time(x_delayed, base_y)
            if total_time <= time_limit:
                return x_delayed, base_y

        # Delay Y
        y_delayed = [0] * delay + base_y
        if not check_collision(base_x, y_delayed, asteroid_x, asteroid_y):
            total_time = calculate_total_time(base_x, y_delayed)
            if total_time <= time_limit:
                return base_x, y_delayed

    # Strategy 6: Two-axis detour (go sideways then proceed)
    for detour_x in range(3, 20):
        for detour_y in range(3, 20):
            for sign_x in [1, -1]:
                for sign_y in [1, -1]:
                    # X detour
                    dx = detour_x * sign_x
                    seq_x1 = generate_sequence(dx, max_pace)
                    seq_x2 = generate_sequence(x_target - dx, max_pace)
                    x_detour = seq_x1 + seq_x2

                    # Y detour
                    dy = detour_y * sign_y
                    seq_y1 = generate_sequence(dy, max_pace)
                    seq_y2 = generate_sequence(y_target - dy, max_pace)
                    y_detour = seq_y1 + seq_y2

                    if not check_collision(x_detour, y_detour, asteroid_x, asteroid_y):
                        total_time = calculate_total_time(x_detour, y_detour)
                        if total_time <= time_limit:
                            return x_detour, y_detour

    # Last resort: return base but warn
    base_time = calculate_total_time(base_x, base_y)
    print(
        f"WARNING: Could not find safe path to ({x_target},{y_target}) avoiding asteroid at ({asteroid_x},{asteroid_y}) within time limit {time_limit}",
        file=sys.stderr,
    )
    print(
        f"WARNING: Returning base solution with time={base_time} (limit={time_limit})",
        file=sys.stderr,
    )
    return base_x, base_y


def solution(data):
    n = int(data[0])
    i = 1

    for _ in range(n):
        # Parse station and time limit
        parts = data[i].split(" ")
        coords = parts[0].split(",")
        x_target = int(coords[0])
        y_target = int(coords[1])
        time_limit = int(parts[1])
        i += 1

        # Parse asteroid position
        asteroid_coords = data[i].split(",")
        asteroid_x = int(asteroid_coords[0])
        asteroid_y = int(asteroid_coords[1])
        i += 1

        # Find safe sequences that avoid asteroid
        x_sequence, y_sequence = find_safe_sequences(
            x_target, y_target, asteroid_x, asteroid_y, time_limit
        )

        # Verify sequences reach target before padding
        if not (
            verify_sequence_reaches_target(x_sequence, x_target)
            and verify_sequence_reaches_target(y_sequence, y_target)
        ):
            print(
                f"ERROR: Sequences don't reach target! X={x_target}, Y={y_target}",
                file=sys.stderr,
            )

        # Pad sequences to same length
        max_len = max(len(x_sequence), len(y_sequence))
        while len(x_sequence) < max_len:
            x_sequence.append(0)
        while len(y_sequence) < max_len:
            y_sequence.append(0)

        # Print sequences
        print(" ".join(map(str, x_sequence)))
        print(" ".join(map(str, y_sequence)))
        print()  # Empty line between cases
