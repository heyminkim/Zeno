#! /usr/bin/env bash

# Evaulation of insertion throughput between RSQF and Zeno.

EXE=insert_throughput
RUN=evaluations/${EXE}
FILE=insert_local

mkdir -p results
rm results/${FILE}.csv

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=ON -DVMEM=OFF -DFIXED=OFF -DBUILD_TESTS=OFF
make ${EXE} -j 8

echo "Running evaluation on Zeno RARRAY r = 1..."
for i in {1..10}; do
    echo ${i}
    ${RUN} -q 16 -f 16 -v 0 -r 1 -s 0 -c 1 -e 12 -l "ZenoR" >> ../results/${FILE}.csv
done

cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=ON -DFIXED=OFF -DBUILD_TESTS=OFF
make ${EXE} -j 8

echo "Running evaluation on Zeno VMEM r = 1..."
for i in {1..10}; do
    echo ${i}
    ${RUN} -q 16 -f 16 -v 0 -r 1 -s 0 -c 1 -e 12 -l "ZenoV" >> ../results/${FILE}.csv
done

cd .. 

echo "Plotting..."
python3 plot.py -s "insert_local" --filenames ${FILE}