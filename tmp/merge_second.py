"""Merge independently validated optimization experiments without regressions."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'python'))
from level6 import parse_patterns
from level5 import simulate
from level6_retime import shorten_startup, remove_idle_ticks

baseline = ROOT / 'python/outputs/level6_before_second_optimization'
destination = ROOT / 'python/outputs/level6_second_candidate'
destination.mkdir(exist_ok=True)
caches = []
for name in ('fourbeam-results', 'fivebeam-results', 'sixbeam-results',
             'sixbeam-medium', 'sixbeam-large', 'sixbeam-enormous',
             'factor-medium64', 'factor-large64', 'dynamic-short',
             'dynamic-two-phase', 'dynamic-three-phase',
             'dynamic-results', 'backward-results', 'sixbeam-toggle-results',
             'sixbeam-toggle-selected', 'erase-results', 'integrated-results', 'noop-results'):
    path = ROOT / 'tmp' / (name + '.json')
    if path.exists():
        try:
            caches.append((name, json.loads(path.read_text())))
        except json.JSONDecodeError:
            print('Cache is being updated; skipped', name)

report = {'datasets': {}, 'baseline_total': 0, 'candidate_total': 0, 'improved_cases': 0}
for input_path in sorted((ROOT / 'level-6').glob('in*.txt')):
    cases = parse_patterns(input_path.read_text(encoding='utf-8-sig'))
    filename = input_path.name.replace('in_', 'out_')
    programs = [[tuple(line.split()) for line in block.splitlines()]
                for block in (baseline / filename).read_text().strip().split('\n\n')]
    assert len(programs) == len(cases)
    old_lengths = list(map(len, programs))
    sources = ['baseline'] * len(cases)
    for source, cache in caches:
        rows = cache.get(input_path.name, [])
        if source in ('fourbeam-results', 'fivebeam-results'):
            candidates = enumerate(rows)
        else:
            candidates = ((row[0], row[3]) for row in rows)
        for index, candidate in candidates:
            if not candidate:
                continue
            r, board, required = cases[index]
            candidate = shorten_startup(r, board, candidate)
            if len(candidate) < len(programs[index]):
                assert all(len(row) == 6 and set(row) <= set('FBT-') for row in candidate)
                assert simulate(r, board, candidate) == required, (source, input_path.name, index)
                programs[index] = candidate
                sources[index] = source
    for i, ((r, board, required), program) in enumerate(zip(cases, programs)):
        programs[i] = remove_idle_ticks(r, board, shorten_startup(r, board, program), required)
        assert simulate(r, board, programs[i]) == required
        assert len(programs[i]) <= old_lengths[i]
    new_lengths = list(map(len, programs))
    before, after = sum(old_lengths), sum(new_lengths)
    wins = sum(a < b for a, b in zip(new_lengths, old_lengths))
    report['datasets'][filename] = {'before': before, 'after': after, 'improved_cases': wins,
                                    'cases': [{'before': old, 'after': new, 'source': source}
                                              for old, new, source in zip(old_lengths, new_lengths, sources)]}
    report['baseline_total'] += before
    report['candidate_total'] += after
    report['improved_cases'] += wins
    result = '\n\n'.join('\n'.join(' '.join(tick) for tick in p) for p in programs) + '\n'
    (destination / filename).write_text(result, encoding='utf-8', newline='\n')
    print(filename, before, '->', after, 'improved', wins, flush=True)
(destination / 'report.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print('TOTAL', report['baseline_total'], '->', report['candidate_total'], flush=True)
