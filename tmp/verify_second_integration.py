import json
import sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from level6 import parse_patterns, generate_program
from level5 import simulate

cache_path = ROOT / 'tmp/integrated-results.json'
records = json.loads(cache_path.read_text()) if cache_path.exists() else {}
for name, indices in [('in_level-6_2-medium.txt', (0, 50, 98, 132, 146, 167, 197)),
                      ('in_level-6_3-large.txt', (2, 26, 143)),
                      ('in_level-6_4-enormous.txt', (0, 1, 64))]:
    cases = parse_patterns((ROOT / 'level-6' / name).read_text(encoding='utf-8-sig'))
    blocks = (ROOT / 'python/outputs/level6_second_candidate' / name.replace('in_', 'out_')).read_text().strip().split('\n\n')
    records.setdefault(name, [])
    for index in indices:
        radius, board, required = cases[index]
        previous = [tuple(line.split()) for line in blocks[index].splitlines()]
        result = generate_program(radius, board, required, attempts=8, incumbent=previous)
        assert len(result) <= len(previous)
        assert simulate(radius, board, result) == required
        if len(result) < len(previous):
            records[name].append([index, len(previous), len(result), result])
        print(name, index, len(previous), '->', len(result), 'PASS', flush=True)
        (ROOT / 'tmp/integrated-results.json').write_text(json.dumps(records))
