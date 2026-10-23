import sys,time
sys.path.insert(0,'python')
from level1 import render_board
from level2 import DIRECTIONS
from level6_multibeam import generate_candidate
from level5 import simulate
from random import Random
rng=Random(938);start=time.time()
for trial in range(300):
    r=rng.randrange(1,9)
    b=[list(row)for row in render_board(r).splitlines()]
    cells={(i,j)for i,row in enumerate(b) for j,x in enumerate(row)if x=='O'}
    target={rng.choice(tuple(cells))}; wanted=rng.randrange(1,len(cells)+1)
    while len(target)<wanted:
        fringe={(i+di,j+dj)for i,j in target for di,dj in DIRECTIONS.values()} & cells - target
        target.add(rng.choice(tuple(fringe)))
    for i,j in target:b[i][j]='M'
    b=[''.join(row)for row in b];k=rng.randrange(1,8)
    program=generate_candidate(r,b,k,attempts=4,rotations=100)
    assert program is not None,(trial,r,len(target))
    count=simulate(r,b,program)
    assert count==k,(trial,r,k,count)
print('PASS',300,'random connected targets, elapsed',time.time()-start)

