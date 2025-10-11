#! /usr/bin/env bash

# Experiment 4
# Evaluation of Zeno filter against Aleph filter for deletion throughput per age and filter size

EXE=exp_void
RUN=bench/${EXE}
FILE=exp_void

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=8
NEXP=16
AGE=16
NREP=10

STR_DELETE="exp_void_delete"
STR_SIZE="exp_void_size"

FN_DELETE="../results/${STR_DELETE}.csv"
FN_SIZE="../results/${STR_SIZE}.csv"

# Remove previous results
for FN in "$FN_DELETE" "$FN_SIZE"; do
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
for ID in 1 2 ; do
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make ${EXE} -j 8

    if [ "$ID" -eq 1 ]; then
        LABEL="VZF"
    elif [ "$ID" -eq 2 ]; then
        LABEL="Aleph"
    fi

    echo "Evaluating ${LABEL}"

    progress_bar 0 $AGE
    for ((j=1; j<=$AGE; j++)); do
        for ((i=1; i<=$NREP; i++)); do
            ${RUN} -q ${QBITS} -f ${FPLEN} -i ${ID} -e ${NEXP} -a ${j} --fn_delete ${FN_DELETE} --fn_size ${FN_SIZE}
        done
        progress_bar $j $AGE
    done
    progress_bar $AGE $AGE
    echo ""

    make clean
done

cd ..

# echo "Plotting..."
# python3 plot.py -s ${FILE} --letter 0 --max_columns 5 --filenames ${STR_DELETE} ${STR_SIZE} # ${STR_DELETE} 