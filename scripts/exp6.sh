#! /usr/bin/env bash

# Experiment 6
# Evaluation of Zeno filter for its multi-threaded performance with concurrent reads and writes

EXE=exp_concurrent_rw
RUN=bench/${EXE}
FILE=exp6_concurrency

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=20
FPLEN=16
NEXP=4
NTHREADS=1

STR_INSERT="exp_concurrent_rw_insert"
STR_QUERY="exp_concurrent_rw_query"

FN_INSERT="../results/${STR_INSERT}.csv"
FN_QUERY="../results/${STR_QUERY}.csv"

# Remove previous results
for FN in "$FN_INSERT" "$FN_QUERY"; do
    rm -f "$FN"
done

echo "onetwothreefour"

# Build
cmake .. -DCMAKE_BUILD_TYPE=Release -DFASTRESIZE=OFF
make ${EXE} -j 8

for ID in 1 2; do
    ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -i ${ID} -t ${NTHREADS} --fn_insert ${FN_INSERT} --fn_query ${FN_QUERY}
done

# Concurrency within period
QBITS=24
FPLEN=16
NEXP=0
NREP=1

EXE=exp_concurrency
RUN=bench/${EXE}
STR_ONE="exp_concurrency_one"

FN_ONE="../results/${STR_ONE}.csv"

# Remove previous results
for FN in "$FN_ONE"; do
    rm -f "$FN"
done

make clean

echo "concurrency one"

# Build
for LS in 4096 65536; do
    cmake .. -DCMAKE_BUILD_TYPE=Release -DLOCKSLOTS=${LS} -DFASTRESIZE=ON
    make ${EXE} -j 8
    for NTHREADS in 1 2 4 8 16 32 64; do
        for ((i=1; i<=NREP; i++)); do
            ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -a 8 -t ${NTHREADS} --fn_insert ${FN_ONE}
        done
    done
done

echo "concurrency"

QBITS=24
FPLEN=16
NEXP=2
NREP=1
THOLD=8

STR_CON_INSERT="exp_concurrency"

FN_CON_INSERT="../results/${STR_CON_INSERT}.csv"

# Remove previous results
for FN in "$FN_CON_INSERT" ; do
    rm -f "$FN"
done

for LS in 4096 65536; do
    cmake .. -DCMAKE_BUILD_TYPE=Release -DLOCKSLOTS=${LS} -DFASTRESIZE=ON
    make ${EXE} -j 8

    # Build
    for NTHREADS in 1 2 4 8 16 32 64; do
        for ((i=1; i<=NREP; i++)); do
            ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -a ${THOLD} -t ${NTHREADS} --fn_insert ${FN_CON_INSERT}
        done
    done
done

make clean
cmake .. -DCMAKE_BUILD_TYPE=Release -DLOCKSLOTS=4096 -DFASTRESIZE=OFF

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 0 --filenames ${STR_ONE} ${STR_CON_INSERT} ${STR_INSERT} ${STR_QUERY} --running_average 50 --individual_legend