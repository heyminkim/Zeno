#! /usr/bin/env bash

# Experiment 8
# Runs script/prepare_dataset.sh and script/exp_wiredtiger.sh

./scripts/prepare_dataset.sh || { echo "Dataset preparation failed; skipping experiment 8." >&2; exit 1; }
./scripts/exp_wiredtiger.sh