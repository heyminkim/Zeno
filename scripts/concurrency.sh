#! /usr/bin/env bash

# Evaulation of Zeno with concurrency. 

EXE=concurrency
RUN=evaluations/${EXE}

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=16
FPLEN=16
NEXP=4
NREP=20
ALPHA=0.8

STR_INSERT="con_insert"    # Stores MOPs of Zeno over time
STR_READ="con_read"        # Stores the total latency

FN_INSERT="../results/${STR_INSERT}.csv"
FN_READ="../results/${STR_READ}.csv"

# Remove previous results
for FN in "$FN_INSERT" "$FN_READ" "FN_DUMMY"; do
    rm -f "$FN"
done

# Build Zeno
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=ON -DRSQF=OFF -DFIXED=OFF -DBUILD_TESTS=OFF

make ${EXE} -j 8

for NTHREAD in 1 2 3 4; do
    echo ${NTHREAD}
    for ((i=1; i<=NREP; i++)); do
        echo ${i}
        ${RUN} -q ${QBITS} -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -a ${ALPHA} -l "Threads${NTHREAD}" -t ${NTHREAD} --fn_in ${FN_INSERT} --fn_rd ${FN_READ}
    done
done

cd ..

echo "Plotting..."
python3 plot.py -s "con" --filenames ${STR_INSERT} ${STR_READ}
