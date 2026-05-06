#! /usr/bin/env bash

# Experiment 4
# Evaulation of directory size with increasing data size. Employs a variant of Zeno Filter
# with a fixed data block size, the directory size of which increases linearly with data size. 

EXE=exp_directory
RUN=bench/${EXE}
FILE=exp4_directory_size

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

STR_SIZE="exp_directory_size"
STR_QUERY="exp_directory_query"

FN_SIZE="../results/${STR_SIZE}.csv"
FN_QUERY="../results/${STR_QUERY}.csv"

# Remove previous results
for FN in "$FN_SIZE" "$FN_QUERY"; do
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
for ID in 1 2; do 

    if [ "$ID" -eq 1 ]; then
        LABEL="IZF"
        PRINTLABEL="Zeno Filter"
    elif [ "$ID" -eq 2 ]; then
        LABEL="FD"
        PRINTLABEL="Zeno Filter with Fixed Datablock Size"
    fi

    echo "Evaluating ${PRINTLABEL}"

    progress_bar 0 $NREP
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -e ${NEXP} -i ${ID} -f ${FPLEN} --fn_size ${FN_SIZE} --fn_query ${FN_QUERY}
        progress_bar $i $NREP
    done
    progress_bar $NREP $NREP
    echo ""
done
make clean

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --individual_legend --filenames ${STR_SIZE} ${STR_QUERY}