#! /usr/bin/env bash

# Experiment 1 (a)
# Evaluation of Zeno filter for its persistent space amplification.

EXE=exp_spaceamp
RUN=bench/${EXE}

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=16
NEXP=4

STR_LF="exp_insert_spaceamp_loadfactor"

FN_LF="../results/${STR_LF}.csv"

# Remove previous results
for FN in "$FN_LF"; do
    rm -f "$FN"
done

make clean
cmake .. -DCMAKE_BUILD_TYPE=Release
make ${EXE} -j 8

# Build
for RRATIO in 1 2 3 4; do
    ${RUN} -q ${QBITS} -f ${FPLEN} -r ${RRATIO} -e ${NEXP} --fn_lf ${FN_LF}
done

make clean

cd ..