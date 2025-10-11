#! /usr/bin/env bash

# Experiment 6
# Evaluation of Zeno filter for its multi-threaded performance. Calls sub-experiments. 

# ./scripts/exp_concurrency_region.sh
# ./scripts/exp_concurrent_rw.sh

FILE=exp_concurrency_all
FILE1=exp_concurrency
FILE2=exp_concurrent_rw_insert

echo "Plotting..."
python3 plot.py -s ${FILE} --letter 0 --running_average 400 --filenames ${FILE1} ${FILE2}