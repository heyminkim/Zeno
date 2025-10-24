#! /usr/bin/env bash

# Experiment 5
# Evaulation of Zeno against different baselines. 

EXE=exp_overall
RUN=bench/${EXE}
FILE=exp5

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=10
NEXP=12
NREP=1

STR_SIZE="exp_overall_size"
STR_PQUERY="exp_overall_pquery"
STR_NQUERY="exp_overall_nquery"
STR_FPR="exp_overall_fpr"
STR_INSERT="exp_overall_insert"

FN_SIZE="../results/${STR_SIZE}.csv"
FN_PQUERY="../results/${STR_PQUERY}.csv"
FN_NQUERY="../results/${STR_NQUERY}.csv"
FN_FPR="../results/${STR_FPR}.csv"
FN_INSERT="../results/${STR_INSERT}.csv"

# Remove previous results
for FN in "$FN_SIZE" "$FN_PQUERY" "$FN_NQUERY" "$FN_FPR" "$FN_INSERT"; do
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
cmake .. -DCMAKE_BUILD_TYPE=Release
make clean
make ${EXE} -j 8
for ID in 1 2 3 4 5 6 7; do 

    if [ "$ID" -eq 1 ]; then
        LABEL="IZF"
    elif [ "$ID" -eq 2 ]; then
        LABEL="IZF2"
    elif [ "$ID" -eq 3 ]; then
        LABEL="VZF"
    elif [ "$ID" -eq 4 ]; then
        LABEL="Aleph"
    elif [ "$ID" -eq 5 ]; then
        LABEL="InfiniFilter"
    elif [ "$ID" -eq 6 ]; then
        LABEL="Bamboo"
    elif [ "$ID" -eq 7 ]; then
        LABEL="LDCF"
    fi

    echo "Evaluating ${LABEL}"

    # Do a dummy run to record the size over the time of execution
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
        ${RUN} -q ${QBITS} -e ${NEXP} -i ${ID} -f ${FPLEN} --fn_nquery ${FN_NQUERY} --fn_fpr ${FN_FPR} --fn_insert ${FN_INSERT}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""
done
make clean

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 0 --legend_column 1 --filenames ${STR_SIZE} ${STR_INSERT} ${STR_NQUERY} ${STR_FPR} 