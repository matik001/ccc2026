import sys,time,json
from pathlib import Path
sys.path.insert(0,'python')
from level6 import parse_patterns
from level6_multibeam import generate_candidate
from level5 import simulate
start=time.time()
result={}
for p in sorted(Path('level-6').glob('in_*.txt')):
    old=Path('python/outputs')/p.name.replace('in_','out_')
    baseline=old.read_text().strip().split('\n\n')
    total=0;wins=0;newtotal=0; rows=[]
    for i,(r,b,k) in enumerate(parse_patterns(p.read_text())):
        n=len(baseline[i].splitlines());total+=n
        program=generate_candidate(r,b,k,attempts=8,budget=n)
        if program is not None:
            actual=simulate(r,b,program)
            if actual!=k: print('BAD',p.name,i,k,actual,flush=True);raise RuntimeError()
            newtotal+=len(program);wins+=1;rows.append([i,n,len(program),program])
        else: newtotal+=n
        if i%50==0: print(p.name,i,'elapsed',round(time.time()-start,1),flush=True)
    print('FILE',p.name,total,newtotal,'wins',wins,'elapsed',round(time.time()-start,1),flush=True)
    result[p.name]=rows
    Path('tmp/multibeam-results.json').write_text(json.dumps(result))
