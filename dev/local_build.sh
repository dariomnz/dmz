#!/bin/bash
set -e

mkdir -p build
cd build
# Only configure cmake if dir is empty
if [ "$(find . -maxdepth 0 -empty -print -quit)" ]; then
    cmake ..
fi
cmake --build . -j $(nproc)

ctest --output-on-failure -V