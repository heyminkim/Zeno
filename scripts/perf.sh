#! /usr/bin/env bash

# Experiment 4
# Evaluation of Zeno filter for its multi-threaded performance with varying expansion threshold

EXE=exp_concurrency
RUN=bench/${EXE}

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=24
FPLEN=16
NEXP=0
NREP=10
THOLD=9     # expansion threshold = 0.8

STR_INSERT="exp_concurrency_perf"
FN_INSERT="../results/${STR_INSERT}.csv"

# RelWithDebInfo
make clean
cmake -DCMAKE_BUILD_TYPE=Release ..
# cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo \ 
#       -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -g" \
#       -DCMAKE_C_FLAGS="-fno-omit-frame-pointer -g" ..
make ${EXE} -j 8

# Build
for ID in 1 2; do
    if [ "$ID" -eq 1 ]; then
        LABEL="VZF"
    elif [ "$ID" -eq 2 ]; then
        LABEL="RSQF"
    fi
    for NTHREADS in 64; do
        echo "Nthreads: ${NTHREADS}"
        for ((i=1; i<=NREP; i++)); do
            ${RUN} -q ${QBITS} -f ${FPLEN} -i ${ID} -e ${NEXP} -a ${THOLD} -t ${NTHREADS} --fn_insert ${FN_INSERT}
            # sudo perf record --call-graph fp -F 99 -g ${RUN} -q ${QBITS} -f ${FPLEN} -i ${ID} -e ${NEXP} -a ${THOLD} -t ${NTHREADS} --fn_insert ${FN_INSERT}
            # sudo perf script > ${LABEL}.perf
            # ~/projects/FlameGraph/stackcollapse-perf.pl ${LABEL}.perf > ${LABEL}.folded
            # ~/projects/FlameGraph/flamegraph.pl ${LABEL}.folded > ${LABEL}.svg
        done
    done
done

make clean

cd ..
