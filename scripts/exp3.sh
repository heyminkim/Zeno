#! /usr/bin/env bash

# Experiment 3
# Evaulation of FPR with large r and widening. 

EXE=exp_widening
RUN=bench/${EXE}
FILE=exp3_widening

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=8
FPLEN=16
NEXP=20
NREP=10

STR_FPR="exp_widening_fpr"
STR_SIZE="exp_widening_size"

FN_FPR="../results/${STR_FPR}.csv"
FN_SIZE="../results/${STR_SIZE}.csv"

# Remove previous results
for FN in "$FN_FPR" "$FN_SIZE"; do
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
for ID in 1 2 3; do
    if [ "$ID" -eq 1 ]; then
        LABEL="VZFnoW"
        PRINTLABEL="Zeno Filter-VM without Widening"
        cmake .. -DCMAKE_BUILD_TYPE=Release -DWIDENING=OFF
        make ${EXE} -j 8
    elif [ "$ID" -eq 2 ]; then
        LABEL="VZFW"
        PRINTLABEL="Zeno Filter-VM with Widening"
        cmake .. -DCMAKE_BUILD_TYPE=Release -DWIDENING=ON
        make ${EXE} -j 8
    elif [ "$ID" -eq 3 ]; then
        LABEL="AlephW"
        PRINTLABEL="Aleph Filter with Widening"
        cmake .. -DCMAKE_BUILD_TYPE=Release -DWIDENING=ON
        make ${EXE} -j 8
    fi

    echo "Evaluating ${PRINTLABEL}"

    # Initial run to measure filter size
    ${RUN} -q ${QBITS} -e ${NEXP} -i ${ID} -f ${FPLEN} &
    pid=$!
    echo -n "${LABEL}," >> ${FN_SIZE}
    while kill -0 "$pid" 2>/dev/null; do
        MEMORY_USAGE=$(ps -o rss= -p $pid | awk '{print $1}')
        if [ -n "$MEMORY_USAGE" ]; then
            MEMORY_USAGE_MB=$(echo "scale=2; $MEMORY_USAGE / 1024" | bc)
            echo -n "${MEMORY_USAGE_MB}," >> ${FN_SIZE}
        fi
        sleep 0.01
    done
    echo "" >> ${FN_SIZE}

    progress_bar 0 $NREP
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -e ${NEXP} -i ${ID} -f ${FPLEN} --fn_fpr ${FN_FPR}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""

    make clean
done

cmake .. -DCMAKE_BUILD_TYPE=Release -DWIDENING=OFF

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --legend_column 1 --filenames ${STR_SIZE} ${STR_FPR}