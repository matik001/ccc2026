import sys,time,json
from pathlib import Path
sys.path.insert(0,'python')
from level6 import parse_patterns
from level6_multibeam import generate_exact_candidate
from level5 import simulate
start=time.time(); result={};cached=json.loads(Path('tmp/multibeam-results64.json').read_text())
for p in sorted(Path('level-6').glob('in_*.txt')):
    baseline=(Path('python/outputs')/p.name.replace('in_','out_')).read_text().strip().split('\n\n')
    prev={i:new for i,old,new,prog in cached[p.name]}
    rows=[];total=0;newtotal=0
    for i,(r,b,k) in enumerate(parse_patterns(p.read_text())):
        n=prev.get(i,len(baseline[i].splitlines()));total+=n
        prog=generate_exact_candidate(r,b,k,n,max_seconds=0.4)
        if prog:
            actual=simulate(r,b,prog)
            assert actual==k,(p.name,i,actual,k)
            rows.append([i,n,len(prog),prog]);newtotal+=len(prog)
            print('WIN',p.name,i,n,len(prog),flush=True)
        else: newtotal+=n
    result[p.name]=rows
    Path('tmp/multibeam-exact.json').write_text(json.dumps(result))
    print('FILE',p.name,total,newtotal,'wins',len(rows),'elapsed',round(time.time()-start,1),flush=True)
