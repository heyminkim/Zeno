#! /usr/bin/env bash

# Experiment 4
# Evaluation of Zeno filter for its multi-threaded performance with varying expansion threshold

EXE=exp_concurrency
RUN=bench/${EXE}
FILE=exp4a

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=24
FPLEN=16
NEXP=2
NREP=1

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

make clean
cmake .. -DCMAKE_BUILD_TYPE=DebugRelease
make ${EXE} -j 8

# Build
for THOLD in 9 8 7 ; do

    for NTHREADS in 1 2 4 8 16 32; do
        echo "Nthreads: ${NTHREADS}"
        progress_bar 0 $NREP
        for ((i=1; i<=NREP; i++)); do
            ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -a ${THOLD} -t ${NTHREADS} --fn_insert ${FN_INSERT}
            progress_bar $i $NREP
        done
        progress_bar $NREP $NREP
        echo ""
    done
    echo ""

done

make clean

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 0  --individual_legend --filenames ${STR_INSERT}