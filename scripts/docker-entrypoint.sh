#! /usr/bin/env bash

# Entry point for the Docker image. Runs the requested experiment scripts ("all" or a
# list such as "exp1 exp6") and leaves figures (*.pdf) and raw data (*.csv) in
# /zeno/results. Bind-mount that directory to collect the results on the host.

cd /zeno || exit 1
mkdir -p results

if [ "$#" -eq 0 ] || [ "$1" = "all" ]; then
    ./scripts/runall.sh
else
    for exp in "$@"; do
        "./scripts/${exp}.sh"
    done
fi

echo "Done. Figures (*.pdf) and raw data (*.csv) are in /zeno/results."
