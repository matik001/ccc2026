import sys, json, time
from pathlib import Path
sys.path.insert(0, 'python')
from level6 import parse_patterns
from level6_backward import generate_candidate
from level5 import simulate

files = sys.argv[1:] or ['in_level-6_4-enormous.txt']
known = {}
for cache_name in ('sixbeam-results.json', 'sixbeam-medium.json', 'sixbeam-large.json',
                   'sixbeam-enormous.json', 'factor-large64.json', 'factor-medium64.json'):
    cache_file = Path('tmp', cache_name)
    if not cache_file.exists():
        continue
    for name, rows in json.loads(cache_file.read_text()).items():
        for index, _, length, _ in rows:
            known[name, index] = min(known.get((name, index), 10**9), length)
cache_file = Path('tmp/fivebeam-results.json')
if cache_file.exists():
    for name, programs in json.loads(cache_file.read_text()).items():
        for index, program in enumerate(programs):
            if program:
                known[name, index] = min(known.get((name, index), 10**9), len(program))
result = {}
start = time.perf_counter()
for name in files:
    cases = parse_patterns(Path('level-6', name).read_text())
    old = [len(block.splitlines()) for block in Path('python/outputs', name.replace('in_', 'out_')).read_text().strip().split('\n\n')]
    indices = [1, 122, 0, 64, 103, 113, 11, 47, 96, 119]
    rows = []
    for index in indices:
        radius, board, required = cases[index]
        bound = min(old[index], known.get((name, index), 10**9))
        candidate = generate_candidate(radius, board, required, attempts=8, budget=bound)
        if candidate:
            assert simulate(radius, board, candidate) == required, (name, index)
            rows.append([index, bound, len(candidate), candidate])
        result[name] = rows
        Path('tmp/backward-results.json').write_text(json.dumps(result))
        print(name, index, bound, len(candidate) if candidate else None,
              'elapsed', round(time.perf_counter() - start, 1), flush=True)
