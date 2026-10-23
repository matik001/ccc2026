import sys,time,json
from pathlib import Path
sys.path.insert(0,'python')
from level6 import parse_patterns
from level6_sixbeam import generate_candidate
from level5 import simulate
start=time.time();result={}
files=sys.argv[1:] or ['in_level-6_0-example.txt','in_level-6_1-small.txt']
for name in files:
    p=Path('level-6')/name
    baseline=(Path('python/outputs')/name.replace('in_','out_')).read_text().strip().split('\n\n')
    total=0;wins=0;newtotal=0; rows=[]
    for i,(r,b,k) in enumerate(parse_patterns(p.read_text())):
        n=len(baseline[i].splitlines());total+=n
        program=generate_candidate(r,b,k,attempts=8,budget=n,rotations=50)
        if program is not None:
            actual=simulate(r,b,program)
            if actual!=k: print('BAD',p.name,i,k,actual,flush=True);raise RuntimeError()
            newtotal+=len(program);wins+=1;rows.append([i,n,len(program),program])
            print('WIN',p.name,i,n,len(program),flush=True)
        else: newtotal+=n
        if i%10==0: print(p.name,i,'elapsed',round(time.time()-start,1),flush=True)
        result[p.name]=rows
        Path('tmp/sixbeam-results.json').write_text(json.dumps(result))
    print('FILE',p.name,total,newtotal,'wins',wins,'elapsed',round(time.time()-start,1),flush=True)
