#! /usr/bin/env bash
set -e

export COMPILER=gcc
export CC=$COMPILER
export LD=$COMPILER

WIREDTIGER_VERSION=${WIREDTIGER_VERSION:-11.3.1}

if [ -d "bench" ]; then
    WT_DIR=bench/wiredtiger
elif [ -d "../bench" ]; then
    WT_DIR=../bench/wiredtiger
else
    echo "Error: Could not find the bench directory"
    exit 1
fi

# The bench/wiredtiger submodule has no gitlink, and CMake pre-creates an empty
# bench/wiredtiger/build/include, so test for the sources themselves and fetch them if absent.
if [ ! -f "$WT_DIR/CMakeLists.txt" ]; then
    echo "WiredTiger sources not found; fetching release ${WIREDTIGER_VERSION}..."
    rm -rf "$WT_DIR.tmp"
    git clone --depth 1 --branch "${WIREDTIGER_VERSION}" \
        https://github.com/wiredtiger/wiredtiger.git "$WT_DIR.tmp"
    rm -rf "$WT_DIR"
    mv "$WT_DIR.tmp" "$WT_DIR"
fi
cd "$WT_DIR"
mkdir -p build && cd build
cmake ..
make -j 8