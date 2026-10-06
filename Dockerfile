# Reproduction environment for the Zeno Filter experiments.
#
# One command builds the image and runs everything (see compose.yaml); figures (PDF) and raw
# data (CSV) land in ./out. Build on the machine you want to measure -- benchmarks compile with
# -march=native:
#   docker compose run --build --rm zeno              # all experiments
#   docker compose run --build --rm zeno exp2 exp6    # a subset
#
# Without Compose:
#   docker build -t zeno .
#   docker run --rm -v "$PWD/out:/zeno/results" -v zeno-data:/zeno/data zeno [exp2 exp6 ...]
#
# Notes:
# - Running every experiment takes many hours; see scripts/ for the individual experiments.
# - Experiment 8 downloads the 3.2 GB SOSD datasets on first run and creates a ~50 GB
#   WiredTiger database inside the container. Cache the datasets across runs with an
#   additional volume: -v zeno-data:/zeno/data

FROM ubuntu:24.04

ARG WIREDTIGER_VERSION=11.3.1
ENV DEBIAN_FRONTEND=noninteractive

# python3-dev and swig are required by WiredTiger's configure step.
# bc and procps (ps) are used by the experiment scripts to sample the filter's RSS.
RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential cmake git ca-certificates \
        libssl-dev \
        python3 python3-dev python3-matplotlib python3-numpy \
        swig wget zstd bc procps \
    && rm -rf /var/lib/apt/lists/*

# Times New Roman, used by plot.py for paper-identical figures. Best effort: if the
# font package cannot be fetched, matplotlib falls back to the default serif font.
RUN echo "ttf-mscorefonts-installer msttcorefonts/accepted-mscorefonts-eula select true" \
        | debconf-set-selections \
    && (apt-get update && apt-get install -y --no-install-recommends \
            ttf-mscorefonts-installer fontconfig && rm -rf /var/lib/apt/lists/* \
        || echo "MS core fonts unavailable; figures will use the fallback serif font")

WORKDIR /zeno

# The bench/wiredtiger submodule has no gitlink in the repository, so fetch a pinned
# release explicitly and pre-build it. Done before copying the sources so that source
# edits do not invalidate this (expensive) layer.
RUN git clone --depth 1 --branch ${WIREDTIGER_VERSION} \
        https://github.com/wiredtiger/wiredtiger.git bench/wiredtiger \
    && cmake -S bench/wiredtiger -B bench/wiredtiger/build \
    && cmake --build bench/wiredtiger/build -j"$(nproc)"

COPY . .

# Smoke-build every experiment binary once to fail at image-build time if the toolchain is
# broken. The experiment scripts reconfigure and rebuild with their own flags at runtime.
# Only the exp_* targets are built: the vendored LDCF baseline ships auxiliary test targets
# that do not compile and are not used by any experiment.
RUN cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_WIREDTIGER=ON \
    && cmake --build build -j"$(nproc)" --target \
        exp_fractional_expansion exp_spaceamp exp_inplace_expansion exp_widening \
        exp_directory exp_contract exp_concurrency exp_concurrent_rw exp_overall \
        exp_wiredtiger \
    && rm -rf build

ENTRYPOINT ["/zeno/scripts/docker-entrypoint.sh"]
CMD ["all"]
