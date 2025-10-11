#! /usr/bin/env bash

# Evaulation of Zeno under fractional expansion. Evaluates (1) filter size, (2) query latency, 
# (3) false positive rate, and (4) overall insertion latency for r = 1, 2, 3, 4. 

EXE=exp_bamboo
RUN=bench/${EXE}
FILE=exp_bamboo

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=16
FPLEN=12
NEXP=12
NREP=10

STR_SIZE="bamboo_size"
STR_PQUERY="bamboo_pquery"
STR_NQUERY="bamboo_nquery"
STR_FPR="bamboo_fpr"
STR_INSERT="bamboo_insert"

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

nslots=$((2**$QBITS))

# nvals = nslots
nvals=$nslots

for i in $(seq 1 $NEXP); do
    nvals=$((nvals * 2))
done

nvals=$((90 * nvals / 100))
nvals=$((nvals + 100000))
data_in_mb=$(((nvals*8)/(1024*1024)))

if [ "$(uname -m)" = "x86_64" ]; then
    IDS="1 2 4 5"
else
    IDS="1 3 4 5 6"
fi

# Build
for ID in $IDS ; do
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make ${EXE} -j 8

    # Use separate thread to calculate filter size if the filter doesn't have API for size
    # if [ "$ID" -eq 1 ] || [ "$ID" -eq 2 ] || [ "$ID" -eq 3 ]; then
    if [ "$ID" -eq 1 ]; then
        LABEL="VZF1"
    elif [ "$ID" -eq 2 ]; then
        LABEL="Bamboo"
    elif [ "$ID" -eq 3 ]; then
        LABEL="LDCF"
    elif [ "$ID" -eq 4 ]; then
        LABEL="Aleph"
    elif [ "$ID" -eq 5 ]; then
        LABEL="RSQF"
    elif [ "$ID" -eq 6 ]; then
        LABEL="E2CF"
    elif [ "$ID" -eq 7 ]; then
        LABEL="EBF"
    fi

    # Do a dummy run to record the size over the time of execution
    ${RUN} -q ${QBITS} -e ${NEXP} -i ${ID} &
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

    echo "Evaluating ${LABEL}"

    progress_bar 0 $NREP
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -e ${NEXP} -i ${ID} --fn_pquery ${FN_PQUERY} --fn_nquery ${FN_NQUERY} --fn_fpr ${FN_FPR} --fn_insert ${FN_INSERT}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""

    make clean
done

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter --max_columns 5 --filenames ${STR_PQUERY} ${STR_NQUERY} ${STR_SIZE} ${STR_FPR} ${STR_INSERT}