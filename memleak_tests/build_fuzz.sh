#!/usr/bin/env bash
set -euo pipefail

python3 -m venv .venv-fuzz
source .venv-fuzz/bin/activate

pip install --upgrade pip setuptools wheel meson-python

#LDFLAGS="-fsanitize=address,undefined" \
CFLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -g" \
LDFLAGS="-fsanitize=address,undefined -Wl,-Map,$(pwd)/pycsh.map"
pip install --no-build-isolation .

echo "Build complete — ready to fuzz."
