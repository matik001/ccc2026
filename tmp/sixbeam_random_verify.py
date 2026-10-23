import sys,time
sys.path.insert(0,'python')
from level1 import render_board
from level2 import DIRECTIONS
from level6_sixbeam import generate_candidate
from level5 import simulate
from random import Random
rng=Random(9873); start=time.time();generated=0
for trial in range(64):
 r=rng.randrange(1,7)
 b=[list(row)for row in render_board(r).splitlines()]
 cells={(i,j)for i,row in enumerate(b) for j,x in enumerate(row)if x=='O'}
 target={rng.choice(tuple(cells))};wanted=rng.randrange(1,len(cells)+1)
 while len(target)<wanted:
  fringe={(i+di,j+dj)for i,j in target for di,dj in DIRECTIONS.values()}&cells-target
  target.add(rng.choice(tuple(fringe)))
 for i,j in target:b[i][j]='M'
 b=[''.join(row)for row in b];k=rng.randrange(1,5)
 program=generate_candidate(r,b,k,attempts=2,rotations=5)
 if program:
  actual=simulate(r,b,program)
  assert actual==k,(trial,r,k,actual)
  generated+=1
print('PASS',generated,'of64 randomcases',round(time.time()-start,2),'seconds')
