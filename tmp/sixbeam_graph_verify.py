import sys,random
sys.path.insert(0,'python')
from level6 import parse_patterns
from level6_factor import enumerate_configs
from level6_sixbeam import SafetyGraph
from pathlib import Path
rng=random.Random(135)
cases=parse_patterns(Path('level-6/in_level-6_2-medium.txt').read_text())
for i in [0,40,110,180,209]:
 r,b,k=cases[i]; states,masks,target=enumerate_configs(r,b);lookup=dict(zip(states,masks));graph=SafetyGraph(r,b)
 for j in range(10000):
  pairs=[sorted(rng.choices(range(-r,r+1),k=2))for _ in range(3)]
  state=tuple(p[0]for p in pairs)+tuple(-p[1]for p in pairs)
  ix=graph.add_state(state)
  assert (ix>=0)==(state in lookup),(i,state,ix,lookup.get(state))
  if ix>=0:assert graph.masks[ix]==lookup[state]
 print('PASS',i,r,len(states))
