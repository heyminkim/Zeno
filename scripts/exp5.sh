#! /usr/bin/env bash

# Experiment 5
# Evaluation of Zeno filter against Aleph filter for filter size after contraction

EXE=exp_contract
RUN=bench/${EXE}
FILE=exp5_contract

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=5
NEXP=8
AGE=1

STR_RATIOC="exp_contract_ratio"
STR_RATIOE="exp_expand_ratio"

FN_RATIOC="../results/${STR_RATIOC}.csv"
FN_RATIOE="../results/${STR_RATIOE}.csv"

# Remove previous results
for FN in "$FN_RATIOC" "$FN_RATIOE"; do
    rm -f "$FN"
done

make clean
cmake .. -DCMAKE_BUILD_TYPE=Debug
make ${EXE} -j 8

# Build
for ID in 1 2; do

    if [ "$ID" -eq 1 ]; then
        LABEL="VZF"
        PRINTLABEL="Zeno Filter-VM"
    elif [ "$ID" -eq 2 ]; then
        LABEL="Aleph"
        PRINTLABEL="Aleph Filter"
    fi

    echo "Evaluating ${PRINTLABEL}"

    ${RUN} -q ${QBITS} -f ${FPLEN} -i ${ID} -e ${NEXP} --fn_delete ${FN_RATIOC} --fn_size2 ${FN_RATIOE}
done

make clean

cd ..

echo "Plotting..."
python3 plot.py -s ${FILE} --no_legend --filenames ${STR_RATIOE} ${STR_RATIOC}