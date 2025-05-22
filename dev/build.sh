#!/bin/bash
set -e

# Only configure cmake if dir is empty
build_dir="./build"

if [ ! -d "${build_dir}" ] || [ -z "$(ls -A "${build_dir}")" ]; then
    cmake -B ${build_dir} -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -GNinja
fi
cmake --build ${build_dir} -j $(nproc) --target check