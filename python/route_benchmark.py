import json
import time
from pathlib import Path

from level6 import parse_patterns
from level6_routes import generate_candidate
from level5 import simulate


started = time.time()
results = {}
root = Path(__file__).resolve().parent.parent
for input_path in sorted((root / 'level-6').glob('in*')):
    cases = parse_patterns(input_path.read_text())
    old_path = root / 'python' / 'outputs' / input_path.name.replace('in_', 'out_')
    old_outputs = old_path.read_text().strip().split('\n\n')
    changes = []
    for index, ((radius, board, required), old) in enumerate(zip(cases, old_outputs)):
        old = [tuple(row.split()) for row in old.splitlines()]
        new = generate_candidate(radius, board, required, incumbent=old)
        if new is not None:
            assert simulate(radius, board, new) == required, (input_path.name, index)
            changes.append((index, len(old) - len(new), new))
    results[input_path.name] = changes
    print(input_path.name, len(changes), 'improvements;', sum(x[1] for x in changes),
          'saved;', round(time.time() - started, 2), 'seconds', flush=True)
(root / 'python' / 'route_candidates.json').write_text(json.dumps(results))
