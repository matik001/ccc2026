#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
for name in level6_trajectories level6_retime; do
  "${CXX:-g++}" -O3 -march=native -std=c++20 -pthread \
    -o "cpp/$name.build.$$" "cpp/$name.cpp"
  mv -f -- "cpp/$name.build.$$" "cpp/$name"
done
