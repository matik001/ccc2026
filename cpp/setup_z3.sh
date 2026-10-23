#!/usr/bin/env bash
# Ubuntu / WSL: unpack Z3 locally, without installing system packages.
set -euo pipefail
cd -- "$(dirname -- "$0")"
mkdir -p vendor
cd vendor
apt-get download libz3-dev libz3-4
for package in libz3-dev_*.deb libz3-4_*.deb; do
  dpkg-deb -x "$package" .
done
