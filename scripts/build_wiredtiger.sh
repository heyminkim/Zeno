#! /usr/bin/env bash
set -e

export COMPILER=gcc
export CC=$COMPILER
export LD=$COMPILER

if [ -d "bench/wiredtiger" ]; then
    cd bench/wiredtiger
elif [ -d "../bench/wiredtiger" ]; then
    cd ../bench/wiredtiger
else
    echo "Error: Could not find bench/wiredtiger directory"
    exit 1
fi
mkdir -p build && cd build
cmake ..
make -j 8