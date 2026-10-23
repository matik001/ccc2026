#!/usr/bin/env bash
# Runda szybkich ulepszen na aktualnych wynikach (~1-2 min):
# scalenie wszystkich kandydatow -> recycle/reverse (cpp/) -> polish -> splice -> scalenie -> weryfikacja.
# Uruchamiac z katalogu glownego repozytorium. PUSH=1 dodatkowo commituje i wypycha.
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
NAMES=2-medium,3-large,4-enormous
merge() { (cd python && python3 level6_merge.py --extra ../cpp/candidates/* | tail -1); }
merge
./cpp/level6_opt recycle baseline=python/outputs names=$NAMES threads=6 tag=_ours progress=0 2>&1 | tail -1
./cpp/level6_opt reverse baseline=python/outputs names=$NAMES threads=6 width=200 tag=_ours progress=0 2>&1 | tail -1
merge
(cd python && ./level6_opt_dev polish names=$NAMES threads=6 iterations=20000 tag=_final 2>&1 | tail -1)
(cd python && python3 level6_splice.py 2-medium 3-large 4-enormous --jobs 6 | tail -1)
merge
node python/verify_level6.cjs | tail -1
if [ "${PUSH:-0}" = 1 ]; then
    git add python/outputs/out_level-6_*.txt python/candidates cpp/candidates
    git commit -q -m "Level 6: runda ulepszen $(node python/verify_level6.cjs | tail -1 | grep -o '[0-9]* instrukcji')" || true
    git push git@github.com:matik001/ccc2026.git master 2>&1 | tail -1
fi
