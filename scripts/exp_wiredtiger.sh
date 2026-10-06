#! /usr/bin/env bash

# Experiment 8
# Evaulation of Zeno Filter and Aleph Filter on wiredtiger.

EXE=exp_wiredtiger
RUN=bench/${EXE}
FILE=exp8_wiredtiger

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=8
FPLEN=16
NREP=1

# Dataset: "books" or "osm". Override with e.g. DATASET=osm ./scripts/exp8.sh
DATASET=${DATASET:-books}
case "$DATASET" in
    books) DATASET_ID=0 ;;
    osm)   DATASET_ID=1 ;;
    *) echo "Unknown DATASET '$DATASET' (expected books or osm)" >&2; exit 1 ;;
esac

STR_SIZE="exp_wiredtiger_size"
STR_INSERT="exp_wiredtiger_insert"

FN_SIZE="../results/${STR_SIZE}.csv"
FN_INSERT="../results/${STR_INSERT}.csv"

# Remove previous results
for FN in "$FN_SIZE" "$FN_INSERT"; do
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
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_WIREDTIGER=ON
make clean
make ${EXE} -j 8
for ID in 0 1 2 3; do 

    if [ "$ID" -eq 0 ]; then
        LABEL="NOFILTER"
    elif [ "$ID" -eq 1 ]; then
        LABEL="IZF"
    elif [ "$ID" -eq 2 ]; then
        LABEL="VZF"
    elif [ "$ID" -eq 3 ]; then
        LABEL="Aleph"
    fi

    echo "Evaluating ${LABEL}"

    progress_bar 0 $NREP
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -i ${ID} -f ${FPLEN} --fn_size ${FN_SIZE} --fn_insert ${FN_INSERT} -d ${DATASET_ID}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""
done
make clean

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --legend_column 1 --filenames ${STR_SIZE} ${STR_INSERT}