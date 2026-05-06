#! /usr/bin/env bash

# Experiment 1
# Runs script/exp_spaceamp.sh and script/exp_stretching.sh

FILE=exp1_stretching
STR_MAXAMP="exp_insert_spaceamp_loadfactor"
STR_AMP="fe_amp"
STR_QUERY="fe_query"
STR_FPR="fe_fpr"

./scripts/exp_spaceamp.sh
./scripts/exp_stretching.sh

echo "Plotting..."
python3 plot.py -s ${FILE} --legend_column 2 --filenames ${STR_MAXAMP} ${STR_AMP} ${STR_QUERY} ${STR_FPR}