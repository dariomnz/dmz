#!/bin/bash
set -e

mkdir -p build
cd build
cmake ..
cmake --build . -j $(nproc)

ctest --output-on-failure -V