import sys,time,json
from pathlib import Path
sys.path.insert(0,'python')
from level6 import parse_patterns
from level6_sixbeam_toggle import generate_candidate
from level5 import simulate
name='in_level-6_4-enormous.txt'; cases=parse_patterns(Path('level-6',name).read_text());base=(Path('python/outputs')/name.replace('in_','out_')).read_text().strip().split('\n\n');old6={i:n for i,o,n,p in json.loads(Path('tmp/sixbeam-enormous.json').read_text())[name]};results=[]
for i in [64,2,10,15,45,0]:
 r,b,k=cases[i];budget=old6.get(i,len(base[i].splitlines()));start=time.time()
 prog=generate_candidate(r,b,k,attempts=2,budget=budget,rotations=10)
 if prog:
  actual=simulate(r,b,prog);assert actual==k,(i,actual,k)
  results.append([i,budget,len(prog),prog])
 print(i,'budget',budget,'new',len(prog)if prog else None,'seconds',round(time.time()-start,2),flush=True)
 Path('tmp/sixbeam-toggle-selected.json').write_text(json.dumps({name:results}))
