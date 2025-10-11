#! /usr/bin/env bash

# Experiment 1
# Evaluation of Zeno filter for its persistent space amplification.

EXE=exp_stretching_spaceamp
RUN=bench/${EXE}
FILE=exp_stretching_spaceamp

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=16
NEXP=8
NREP=1

STR_LF="exp_stretching_spaceamp"

FN_LF="../results/${STR_LF}.csv"

# Remove previous results
for FN in "$FN_LF"; do
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
for RRATIO in 1 2 3 4; do
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make ${EXE} -j 8

    progress_bar 0 $NREP
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${FPLEN} -r ${RRATIO} -e ${NEXP} --fn_lf ${FN_LF}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""

make clean
done

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter -1 --wideplot --max_columns 5 --filenames ${STR_LF}