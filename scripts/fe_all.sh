#! /usr/bin/env bash

# Evaulation of Aleph, Zeno, and RSQF under fractional expansion. 

EXE=fractional_expansion_
RUN=evaluations/${EXE}
FILE=fe_all

mkdir -p results

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=12
FPLEN=16
NEXP=$((1*FPLEN))
NREP=20

STR_INSERT="fe_insert"
STR_QUERY="fe_query"
STR_FPR="fe_fpr"
STR_AMP="fe_amp"

FN_INSERT="../results/${STR_INSERT}.csv"
FN_QUERY="../results/${STR_QUERY}.csv"
FN_FPR="../results/${STR_FPR}.csv"
FN_AMP="../results/${STR_AMP}.csv"

# Remove previous results
for FN in "$FN_INSERT" "$FN_QUERY" "$FN_FPR" "$FN_AMP"; do
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

# Build Zeno
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=ON -DRSQF=OFF -DWIDENING=OFF

for ZER in 1 2 3 4; do
    make ${EXE} -j 8
    echo "Running evaluation on Zeno VMEM r = ${ZER}..."

    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${FPLEN} -v 0 -r ${ZER} -s 0 -c 1 -e ${NEXP} -l "Zeno${ZER}" --fn_insert ${FN_INSERT} --fn_query ${FN_QUERY} --fn_fpr ${FN_FPR} --fn_amp ${FN_AMP}
        progress_bar $i $NREP
    done

    progress_bar $NREP $NREP
    echo ""
    make clean
done

# Build Aleph
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=OFF -DRSQF=OFF -DWIDENING=OFF

for AFL in ${FPLEN} $((FPLEN-1)); do
    make ${EXE} -j 8
    echo "Running evaluation on Aleph f = ${AFL}..."

    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${AFL} -v 0 -r 0 -s 0 -c 1 -e ${NEXP} -l "Aleph${AFL}" --fn_insert ${FN_INSERT} --fn_query ${FN_QUERY} --fn_fpr ${FN_FPR} --fn_amp ${FN_AMP}
        progress_bar $i $NREP
    done

    progress_bar $NREP $NREP
    echo ""
    make clean
done

# Build RSQF
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=OFF -DRSQF=ON -DWIDENING=OFF

make ${EXE} -j 8
echo "Running evaluation on RSQF f = ${FPLEN}..."

for ((i=1; i<=NREP; i++)); do
    ${RUN} -q ${QBITS} -f ${FPLEN} -v 0 -r 0 -s 0 -c 1 -e ${NEXP} -l "RSQF${FPLEN}" --fn_insert ${FN_INSERT} --fn_query ${FN_QUERY} --fn_fpr ${FN_FPR} --fn_amp ${FN_AMP}
    progress_bar $i $NREP
done
progress_bar $NREP $NREP
echo ""
make clean

cd ..

echo "Plotting..."
python3 plot.py -s "fe_all" --letter --filenames ${STR_INSERT} ${STR_QUERY} ${STR_FPR} ${STR_AMP}