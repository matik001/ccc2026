#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
if [[ ! -f cpp/vendor/usr/include/z3++.h ]]; then
  echo 'Missing local Z3 dependency. Run: bash cpp/setup_z3.sh' >&2
  exit 1
fi
for name in level6_sat level6_sat_cycle; do
  target="cpp/$name"
  g++ -O3 -march=native -std=c++20 -pthread -Icpp/vendor/usr/include \
    -o "$target.build.$$" "$target.cpp" -Lcpp/vendor/usr/lib/x86_64-linux-gnu \
    '-Wl,-rpath,$ORIGIN/vendor/usr/lib/x86_64-linux-gnu' -lz3
  mv -f -- "$target.build.$$" "$target"
done
