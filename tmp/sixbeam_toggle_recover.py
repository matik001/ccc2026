import sys,json
sys.path.insert(0,'python')
from pathlib import Path
from level6 import parse_patterns
from level6_sixbeam_toggle import generate_candidate
from level5 import simulate
name='in_level-6_4-enormous.txt';r,b,k=parse_patterns(Path('level-6',name).read_text())[15]
p=generate_candidate(r,b,k,attempts=2,budget=58,rotations=10,bridges=False)
assert simulate(r,b,p)==k
path=Path('tmp/sixbeam-toggle-selected.json');cache=json.loads(path.read_text());cache[name]=[row for row in cache[name]if row[0]!=15]+[[15,58,len(p),p]];path.write_text(json.dumps(cache));print(len(p))
