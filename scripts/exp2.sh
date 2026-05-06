#! /usr/bin/env bash

# Experiment 2
# Evaulation of Zeno under in-place expansion. Evaluates (1) filter size, (2) insert latency,
# (3) query latency, and (4) fpr. Compares Zeno filter / Zeno filter-VM, Aleph filter, and 
# Aleph filter (10-bits). 

EXE=exp_inplace_expansion
RUN=bench/${EXE}
FILE=exp2_inplace_expansion

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

STR_SIZE="ie_size"
STR_QUERY="ie_query"
STR_FPR="ie_fpr"
STR_INSERT="ie_insert"

FN_SIZE="../results/${STR_SIZE}.csv"
FN_QUERY="../results/${STR_QUERY}.csv"
FN_FPR="../results/${STR_FPR}.csv"
FN_INSERT="../results/${STR_INSERT}.csv"

# Remove previous results
for FN in "$FN_SIZE" "$FN_QUERY" "$FN_FPR" "$FN_INSERT"; do
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

# ID | Description
for ID in 1 2 3 4; do
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make ${EXE} -j 8

    if [ "$ID" -eq 1 ]; then
        LABEL="IZFie1"
        PRINTLABEL="Zeno Filter"
    elif [ "$ID" -eq 2 ]; then
        LABEL="VZFie1"
        PRINTLABEL="Zeno Filter-VM"
    elif [ "$ID" -eq 3 ]; then
        LABEL="Alephie16"
        PRINTLABEL="Aleph Filter"
    elif [ "$ID" -eq 4 ]; then
        LABEL="Alephie10"
        PRINTLABEL="Aleph Filter (f=10)"
        FPLEN=10
    fi

    echo "Evaluating ${PRINTLABEL}"

    # Initial run to measure filter size
    ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -i ${ID} -l ${LABEL} &
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

    # Successive runs to measure insert/query latency and fpr
    progress_bar 0 $NREP
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -i ${ID} -l ${LABEL} --fn_query ${FN_QUERY} --fn_fpr ${FN_FPR} --fn_insert ${FN_INSERT}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""

    make clean
done

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --legend_column 1 --filenames ${STR_SIZE} ${STR_INSERT} ${STR_QUERY} ${STR_FPR}