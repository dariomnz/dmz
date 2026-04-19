#!/bin/bash
set -e

# Only configure cmake if dir is empty
build_dir="./build"

if [ ! -d "${build_dir}" ] || [ -z "$(ls -A "${build_dir}")" ]; then
    cmake -B ${build_dir} -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_EXPORT_COMPILE_COMMANDS=1
    # -DCMAKE_CXX_COMPILER_LAUNCHER=time
fi

FLAGS=""
for arg in "$@"; do
    if [ "$arg" == "clean" ]; then
        FLAGS="--clean-first"
        break
    fi
done
cmake --build ${build_dir} -j $(nproc) --target dmz ${FLAGS}
# cmake --build ${build_dir} --target dmz ${FLAGS}

for arg in "$@"; do
    if [ "$arg" == "test" ]; then
        ./build/bin/dmz -test-compiler ./test/ -j 0
        break
    fi
done