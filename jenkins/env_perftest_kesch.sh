#!/bin/bash

source ${JENKINSPATH}/machine_env.sh

module load matplotlib/1.4.3-gmvolf-15.11-Python-2.7.10

export GRIDTOOLS_BUILD_PATH=/scratch/jenkins/workspace/
export STELLA_BUILD_PATH=/project/c14/install/${myhost}

export CUDA_AUTO_BOOST=0
export GCLOCK=875
export G2G=1
export DEFAULT_QUEUE=debug
export CPUS_PER_SOCKET=12