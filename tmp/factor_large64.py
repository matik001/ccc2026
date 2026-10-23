import sys, json, time
from pathlib import Path
sys.path.insert(0, 'python')
from level6 import parse_patterns
from level6_sixbeam import generate_candidate
from level5 import simulate

name = 'in_level-6_3-large.txt'
cases = parse_patterns(Path('level-6', name).read_text())
old = [len(block.splitlines()) for block in Path('python/outputs', name.replace('in_', 'out_')).read_text().strip().split('\n\n')]
for index, _, length, _ in json.loads(Path('tmp/sixbeam-large.json').read_text())[name]:
    old[index] = min(old[index], length)
five = json.loads(Path('tmp/fivebeam-results.json').read_text()).get(name, [])
for index, program in enumerate(five):
    if program:
        old[index] = min(old[index], len(program))
rows = []
start = time.perf_counter()
for index in (26, 54, 118, 127, 141, 143, 172, 184):
    radius, board, required = cases[index]
    candidate = generate_candidate(radius, board, required, attempts=64, budget=old[index], rotations=200)
    if candidate is not None:
        assert simulate(radius, board, candidate) == required, index
        rows.append([index, old[index], len(candidate), candidate])
    print(index, old[index], len(candidate) if candidate else None,
          'elapsed', round(time.perf_counter() - start, 1), flush=True)
    Path('tmp/factor-large64.json').write_text(json.dumps({name: rows}))
