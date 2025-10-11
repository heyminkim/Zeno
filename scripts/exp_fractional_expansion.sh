#! /usr/bin/env bash

# Evaulation of Zeno under fractional expansion. Evaluates (1) filter size, (2) query latency, 
# (3) false positive rate, and (4) overall insertion latency for r = 1, 2, 3, 4. 

EXE=exp_fractional_expansion
RUN=bench/${EXE}
FILE=exp_fe

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=16
NEXP=16
NREP=1

STR_SIZE="fe_insert"
STR_QUERY="fe_query"
STR_FPR="fe_fpr"
STR_AMP="fe_amp"

FN_SIZE="../results/${STR_SIZE}.csv"
FN_QUERY="../results/${STR_QUERY}.csv"
FN_FPR="../results/${STR_FPR}.csv"
FN_AMP="../results/${STR_AMP}.csv"

# Remove previous results
for FN in "$FN_SIZE" "$FN_QUERY" "$FN_FPR" "$FN_AMP"; do
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
make ${EXE} -j 8

for ((i=1; i<=NREP; i++)); do
    ${RUN} -q ${QBITS} -f $((FPLEN-1)) -e ${NEXP} -r 1 --fn_size ${FN_SIZE} --fn_query ${FN_QUERY} --fn_fpr ${FN_FPR} --fn_amp ${FN_AMP}
    progress_bar $i $NREP
done
progress_bar $NREP $NREP
echo ""

make clean

# ID | Description
# 1  | Zeno with r = 1
# 2  | Zeno with r = 2
# 3  | Zeno with r = 3
# 4  | Zeno with r = 4
for ID in 1 2 3 4; do
    cmake .. -DCMAKE_BUILD_TYPE=Release
    make ${EXE} -j 8

    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${FPLEN} -e ${NEXP} -r ${ID} --fn_size ${FN_SIZE} --fn_query ${FN_QUERY} --fn_fpr ${FN_FPR} --fn_amp ${FN_AMP}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""

    make clean
done

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 0 --filenames ${STR_SIZE} ${STR_QUERY} ${STR_FPR} ${STR_AMP}