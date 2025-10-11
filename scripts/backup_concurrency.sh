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

QBITS=30
FPLEN=16
NEXP=1
NREP=1
ALPHA=0.8

STR_THRUPUT="con_throughput"    # Stores MOPs of Zeno over time
STR_OVERALL="con_overall"       # Stores the total latency
STR_DUMMY="con_dummy"           # Dummy file

FN_THRUPUT="../results/${STR_THRUPUT}.csv"
FN_OVERALL="../results/${STR_OVERALL}.csv"
FN_DUMMY="../results/${STR_DUMMY}.csv"

# Remove previous results
for FN in "$FN_THRUPUT" "$FN_OVERALL" "FN_DUMMY"; do
    rm -f "$FN"
done

# Build Zeno
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=ON -DRSQF=OFF -DFIXED=OFF -DBUILD_TESTS=OFF

make ${EXE} -j 8

for NTHREAD in 0 1 2 3 4; do
    echo ${NTHREAD}
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -a ${ALPHA} -l "Threads${NTHREAD}" -t ${NTHREAD} --fn_tp ${FN_THRUPUT} --fn_oa ${FN_OVERALL} --fn_dm ${FN_DUMMY}
    done
done

cd ..

echo "Plotting..."
python3 plot.py -s "con" --filenames ${STR_THRUPUT} ${STR_OVERALL} --running_average 1
