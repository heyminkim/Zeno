#! /usr/bin/env bash

# Experiment 6?
# Evaluation of Zeno filter for its multi-threaded performance with varying region size

EXE=exp_concurrency
RUN=bench/${EXE}
FILE=exp_concurrency

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=25
FPLEN=16
NEXP=0
NREP=10

STR_INSERT="exp_concurrency"
STR_LF="exp_concurrency"

FN_INSERT="../results/${STR_INSERT}.csv"
FN_LF="../results/${STR_LF}.csv"

# Remove previous results
for FN in "$FN_INSERT" "$FN_LF"; do
    rm -f "$FN"
done

# Progress bar
progress_bar()
{
    elapsed=$1
    total=$2
    percent=$((100*elapsed/total))
    bar_width=50
    filled=$((bar_width*percent/100))
    empty=$((bar_width-filled))
    
    bar=""
    space=""

    if ((filled>0)); then
        bar=$(printf "%0.s#" $(seq 1 $filled))
    fi
    if ((empty>0)); then
        space=$(printf "%0.s-" $(seq 1 $empty))
    fi
    printf "\r[%s%s] %d%%" "$bar" "$space" "$percent"
}

# Build
for REGION in 8 12 16; do
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make ${EXE} -j 8

    for NTHREADS in 1 2 4 8 16; do
        echo "Nthreads: ${NTHREADS}"
        progress_bar 0 $NREP
        for ((i=1; i<=NREP; i++)); do
            ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -r ${REGION} -t ${NTHREADS} --fn_insert ${FN_INSERT}
            progress_bar $i $NREP
        done
        progress_bar $NREP $NREP
        echo ""
    done
    echo ""

    make clean
done

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 0 --filenames ${STR_INSERT}