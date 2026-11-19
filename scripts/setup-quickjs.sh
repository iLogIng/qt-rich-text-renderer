#!/usr/bin/env bash
set -euo pipefail

# Project's Root
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

if [[ ! -f third_party/quickjs-ng/CMakeLists.txt ]]; then
  git submodule update --init --recursive third_party/quickjs-ng
fi

# Compile and Setup Quickjs
cmake -S third_party/quickjs-ng -B third_party/quickjs-ng/build \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_SHARED_LIBS=OFF \
  -DQJS_BUILD_LIBC=ON
cmake --build third_party/quickjs-ng/build --target qjs_exe -j"$(nproc)"

third_party/quickjs-ng/build/qjs --version
