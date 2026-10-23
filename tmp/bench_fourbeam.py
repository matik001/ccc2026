import json, sys, time
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from level6 import parse_patterns
from level6_fourbeam import generate_candidate

root = Path(__file__).resolve().parents[1]
results = {}
started = time.time()
for p in sorted((root / 'level-6').glob('in*.txt')):
    cases = parse_patterns(p.read_text(encoding='utf-8-sig'))
    blocks = (root / 'python/outputs' / p.name.replace('in_', 'out_')).read_text().strip().split('\n\n')
    records = []
    results[p.name] = records
    saved = wins = 0
    for i, (r, b, n) in enumerate(cases):
        old = len(blocks[i].splitlines())
        candidate = generate_candidate(r, b, n, attempts=16, budget=old)
        records.append(candidate)
        if candidate:
            saved += old - len(candidate)
            wins += 1
        if i % 20 == 0:
            print(p.name, i, 'saved', saved, 'seconds', round(time.time()-started, 1), flush=True)
            (root / 'tmp/fourbeam-results.json').write_text(json.dumps(results))
    print(p.name, 'TOTAL SAVED', saved, 'WINS', wins, flush=True)
    (root / 'tmp/fourbeam-results.json').write_text(json.dumps(results))
