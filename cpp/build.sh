#!/usr/bin/env bash
set -euo pipefail
cd -- "$(dirname -- "$0")/.."
"${CXX:-g++}" -O3 -march=native -std=c++20 -pthread -Wall -Wextra \
  -o "cpp/level6_opt.build.$$" cpp/level6_opt.cpp
mv -f -- "cpp/level6_opt.build.$$" cpp/level6_opt
"${CXX:-g++}" -O3 -march=native -std=c++20 -pthread \
  -o "cpp/level6_splice.build.$$" cpp/level6_splice.cpp
mv -f -- "cpp/level6_splice.build.$$" cpp/level6_splice
