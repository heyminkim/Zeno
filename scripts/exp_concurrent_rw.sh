#! /usr/bin/env bash

# Experiment 6?
# Evaluation of Zeno filter for its multi-threaded performance with concurrent reads and writes

EXE=exp_concurrent_rw
RUN=bench/${EXE}
FILE=exp_concurrent_rw

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=25
FPLEN=16
NEXP=1
NTHREADS=1
NREP=10

STR_INSERT="exp_concurrent_rw_insert"

FN_INSERT="../results/${STR_INSERT}.csv"

# Remove previous results
for FN in "$FN_INSERT"; do
    rm -f "$FN"
done

# Build
cmake .. -DCMAKE_BUILD_TYPE=Release
make ${EXE} -j 8

for ((i=1; i<=NREP; i++)); do
    ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -t ${NTHREADS} --fn_insert ${FN_INSERT}
done

cd ..

python3 reorder.py

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 1 --filenames ${STR_INSERT} --running_average 400