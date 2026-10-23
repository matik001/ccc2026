import sys, json, time
from pathlib import Path
sys.path.insert(0, 'python')
from level6 import parse_patterns
from level6_sixbeam import generate_candidate
from level5 import simulate

start = time.perf_counter()
caches = {}
for file in ('sixbeam-results.json', 'sixbeam-medium.json'):
    source = Path('tmp', file)
    if source.exists():
        for name, rows in json.loads(source.read_text()).items():
            for index, old, new, program in rows:
                caches[name, index] = program
for file in ('fivebeam-results.json',):
    source = Path('tmp', file)
    if source.exists():
        for name, programs in json.loads(source.read_text()).items():
            for index, program in enumerate(programs):
                if program and ((name, index) not in caches or len(program) < len(caches[name, index])):
                    caches[name, index] = program

result = {}
for name in ('in_level-6_0-example.txt', 'in_level-6_1-small.txt', 'in_level-6_2-medium.txt'):
    baseline = Path('python/outputs', name.replace('in_', 'out_')).read_text().strip().split('\n\n')
    rows = []
    total = improved = saved = 0
    for index, (radius, board, required) in enumerate(parse_patterns(Path('level-6', name).read_text())):
        old = len(baseline[index].splitlines())
        if (name, index) in caches:
            old = min(old, len(caches[name, index]))
        total += old
        candidate = generate_candidate(radius, board, required, attempts=64,
                                       budget=old, rotations=200)
        if candidate is not None:
            assert simulate(radius, board, candidate) == required, (name, index)
            improved += 1
            saved += old - len(candidate)
            rows.append([index, old, len(candidate), candidate])
            print('WIN', name, index, old, len(candidate), flush=True)
        if index % 20 == 0:
            print('PROGRESS', name, index, 'elapsed', round(time.perf_counter() - start, 1), flush=True)
        result[name] = rows
        Path('tmp/factor-medium64.json').write_text(json.dumps(result))
    print('TOTAL', name, total, total - saved, 'wins', improved,
          'elapsed', round(time.perf_counter() - start, 1), flush=True)
