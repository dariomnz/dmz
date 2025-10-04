#!/bin/bash
set -e

# Only configure cmake if dir is empty
build_dir="./build"

if [ ! -d "${build_dir}" ] || [ -z "$(ls -A "${build_dir}")" ]; then
    cmake -B ${build_dir} -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=1
fi
# cmake --build ${build_dir} -j $(($(nproc)/2)) --target check
if [ "$1" == "compiler" ]; then
    TARGET="compiler"
else
    TARGET="check"
fi
cmake --build ${build_dir} -j $(nproc) --target $TARGET
# cmake --build ${build_dir} --target check