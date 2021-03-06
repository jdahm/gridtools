#!/bin/bash

source $(dirname "$BASH_SOURCE")/kesch.sh

export G2G=2
export MV2_USE_GPUDIRECT=0
export MV2_USE_RDMA_FAST_PATH=0

export CTEST_PARALLEL_LEVEL=1

export GTCMAKE_GT_ENABLE_BACKEND_CUDA=ON
export GTCMAKE_GT_ENABLE_BACKEND_X86=OFF
export GTCMAKE_GT_ENABLE_BACKEND_MC=OFF
export GTCMAKE_GT_ENABLE_BACKEND_NAIVE=OFF
export GTCMAKE_GT_EXAMPLES_FORCE_CUDA=ON
