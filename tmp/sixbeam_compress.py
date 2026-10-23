"""Remove redundant stationary exposures while retaining the exact delivery count."""
from level2 import DIRECTIONS
from level4 import border_positions

def compress_ticks(radius, board, program, required):
    cells={p:i for i,p in enumerate((r,c) for r,row in enumerate(board)for c,symbol in enumerate(row)if symbol in 'OM')}
    targetcells={p for p in cells if board[p[0]][p[1]]=='M'}
    halo={(r+dr,c+dc)for r,c in targetcells for dr,dc in DIRECTIONS.values()}-targetcells
    target=sum(1<<cells[p]for p in targetcells)
    forbidden=sum(1<<cells[p]for p in halo if p in cells)
    border=border_positions(radius);raycache={}
    def ray(e,pos):
        key=(e,pos)
        if key not in raycache:
            r,c=border[pos];dr,dc=DIRECTIONS[str(e+1)];m=0
            r+=dr;c+=dc
            while 0<=r<len(board) and 0<=c<len(board[0]):
                if (r,c)in cells:m|=1<<cells[r,c]
                r+=dr;c+=dc
            raycache[key]=m
        return raycache[key]
    positions=[i*radius for i in range(6)];active=[False]*6;exposures=[]
    for tick in program:
        for e,op in enumerate(tick):
            if op=='F':positions[e]=(positions[e]+1)%len(border)
            elif op=='B':positions[e]=(positions[e]-1)%len(border)
            elif op=='T':active[e]=not active[e]
        one=two=many=0
        for e,on in enumerate(active):
            if on:
                r=ray(e,positions[e]);many|=two&r;two|=one&r;one|=r
        exposures.append((two&~many,many))
    original_length=len(program)
    program=list(program)
    def count(skip_start=-1,skip_end=-1):
        matter=0;done=0
        for i,(create,destroy)in enumerate(exposures):
            if skip_start<=i<skip_end:continue
            matter=(matter|create)&~destroy
            if matter&target==target and not matter&forbidden:
                done+=1;matter&=~target
        return done
    assert count()==required
    i=0
    while i<len(program):
        if all(op=='-'for op in program[i]) and count(i,i+1)==required:
            del program[i];del exposures[i]
            i=max(0,i-6);continue
        movements=[0]*6;toggles=[0]*6;candidates=[]
        for j in range(i,min(len(program),i+7)):
            for e,op in enumerate(program[j]):
                if op=='F':movements[e]+=1
                elif op=='B':movements[e]-=1
                elif op=='T':toggles[e]^=1
            if j==i:continue
            if all(abs(d)<=1 and (not d or not t) for d,t in zip(movements,toggles)):
                ops=tuple('T'if t else 'F'if d>0 else 'B'if d<0 else '-'for d,t in zip(movements,toggles))
                candidates.append((j,ops))
        for j,ops in reversed(candidates):
            if count(i,j)==required:
                program[i:j+1]=[ops]
                exposures[i:j+1]=[exposures[j]]
                i=max(0,i-6)
                break
        else:
            i+=1
    return program if len(program)<original_length else None

if __name__=='__main__':
    from pathlib import Path
    import time,json
    from level6 import parse_patterns
    from level5 import simulate
    result={};start=time.time();savings=0
    for p in sorted(Path('level-6').glob('in_*.txt')):
        programs=(Path('python/outputs/level6_second_candidate')/p.name.replace('in_','out_')).read_text().strip().split('\n\n');rows=[]
        for i,(r,b,k)in enumerate(parse_patterns(p.read_text())):
            prog=[tuple(line.split())for line in programs[i].splitlines()]
            candidate=compress_ticks(r,b,prog,k)
            if candidate:
                assert simulate(r,b,candidate)==k
                rows.append([i,len(prog),len(candidate),candidate]);savings+=len(prog)-len(candidate)
        result[p.name]=rows
        print(p.name,len(rows),sum(o-n for i,o,n,prog in rows),flush=True)
    Path('tmp/sixbeam-compress.json').write_text(json.dumps(result));print('SAVED',savings,'SECONDS',time.time()-start)


