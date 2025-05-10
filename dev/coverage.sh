#!/bin/bash
set -e
set -x

cmake -B ./build -DCMAKE_BUILD_TYPE=Coverage -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build ./build -j $(nproc)

cmake --build ./build -t check

mkdir coverage_report
COV_FILE=./coverage_report/compiler.profdata
llvm-profdata merge -output=${COV_FILE} $(find ./test -name *.profraw)

llvm-cov show ./build/bin/compiler -instr-profile=${COV_FILE} -format=html -output-dir=coverage_report
llvm-cov show ./build/bin/compiler -instr-profile=${COV_FILE} -format=text -output-dir=coverage_report