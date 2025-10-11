#! /usr/bin/env bash

# Evaulation of Zeno's cluster length under different load factors. 

EXE=cluster
RUN=evaluations/${EXE}
FILE=cluster

mkdir -p results
rm results/${FILE}.csv

if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build || exit

QBITS=24
FPLEN=16
NEXP=1
NREP=1

# Build Zeno
cmake .. -DCMAKE_BUILD_TYPE=Release -DRARRAY=OFF -DVMEM=ON -DRSQF=OFF -DFIXED=OFF -DBUILD_TESTS=OFF

make ${EXE} -j 8

# 50 70 80 85 90 95 96 97 98 99
for ALPHA in 50 70 80 85 90 95; do
    for ((i=1; i<=NREP; i++)); do
        ${RUN} -q ${QBITS} -f ${FPLEN} -v 0 -r 1 -s 0 -c 1 -e ${NEXP} -l "${ALPHA}" -a ${ALPHA} >> ../results/${FILE}.csv 
    done
done

cd ..

echo "Plotting..."
python3 plot_box.py -s "cluster" --filenames ${FILE}
