#!/bin/bash
set -e

# Only configure cmake if dir is empty
build_dir="./build"

GENERATOR="Unix Makefiles"
if command -v ninja &> /dev/null
then
   GENERATOR="Ninja"
fi

if [ ! -d "${build_dir}" ] || [ -z "$(ls -A "${build_dir}")" ]; then
    cmake -B ${build_dir} -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -G"${GENERATOR}"
fi
cmake --build ${build_dir} -j $(($(nproc)*2)) --target check