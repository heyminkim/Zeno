#! /usr/bin/env bash
set -e

# Evaulation of Zeno on FPR under fixed and widening regime. 

EXE=fpr
RUN=evaluations/${EXE}
FILE=widening_fpr_zeno

mkdir -p results
rm -f results/${FILE}.csv

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

FPLEN=16
NEXP=$((1*FPLEN+2))
NREP=20

# Progress bar
progress_bar() 
{
    elapsed=$(( $(date +%s) - start_global ))
    percent=$(( 100 * elapsed / estimated_total_time ))
    if (( percent > 100 )); then
        percent=100
    fi
    bar_width=50
    filled=$(( bar_width * percent / 100 ))
    empty=$(( bar_width - filled ))
    bar=$(printf "%0.s#" $(seq 1 $filled))
    space=$(printf "%0.s-" $(seq 1 $empty))
    printf "\r[%s%s] %d%%" "$bar" "$space" "$percent"
}

# Build Zeno RARRAY
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=ON -DVMEM=OFF -DRSQF=OFF -DWIDENING=ON -DFIXED=OFF -DBUILD_TESTS=OFF

make ${EXE} -j 8
echo "Running evaluation on Zeno RARRAY r = 1..."
start_time=$(date +%s)
${RUN} -q 8 -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -l "ZenoRW" >> ../results/${FILE}.csv
end_time=$(date +%s)

first_duration=$((end_time-start_time))
estimated_total_time=$((first_duration*(NREP)))
start_global=$(date +%s)

for ((i=2; i<=NREP; i++)); do
    ${RUN} -q 8 -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -l "ZenoRW" >> ../results/${FILE}.csv
    progress_bar
done
percent=100
filled=50
empty=0
bar=$(printf "%0.s#" $(seq 1 $filled))
printf "\r[%s] %d%%" "$bar" "$percent"
echo ""
make clean

# Build Zeno VMEM
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=ON -DRSQF=OFF -DWIDENING=ON -DFIXED=OFF -DBUILD_TESTS=OFF

make ${EXE} -j 8
echo "Running evaluation on Zeno VMEM r = 1..."
start_time=$(date +%s)
${RUN} -q 8 -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -l "ZenoVW" >> ../results/${FILE}.csv
end_time=$(date +%s)

first_duration=$((end_time-start_time))
estimated_total_time=$((first_duration*(NREP)))
start_global=$(date +%s)

for ((i=2; i<=NREP; i++)); do
    ${RUN} -q 8 -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -l "ZenoVW" >> ../results/${FILE}.csv
    progress_bar
done
percent=100
filled=50
empty=0
bar=$(printf "%0.s#" $(seq 1 $filled))
printf "\r[%s] %d%%" "$bar" "$percent"
echo ""
make clean

cd ..

echo "Plotting..."
python3 plot.py -s "zeno_widening" --filenames ${FILE}