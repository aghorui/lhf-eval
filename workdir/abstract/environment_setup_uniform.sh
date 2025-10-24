#!/bin/bash
# set -o xtrace
set -o errexit
set -o nounset

# This file lists the test build configurations as well as the test variable
# constants. They can be changed as per requirement.

export TEST_ROOT="../"
export TEST_INCLUDE_DIR="$TEST_ROOT/include"
export TEST_LHF_INCLUDE_DIR="$TEST_ROOT/../lhf/src/include"
export TEST_SRC_FILE_NAME="test.cpp"
export TEST_EXEC_FILE_NAME="test_out"
export TEST_DATA_FILE_NAME="test_data.txt"
export TEST_BENCHMARK_SCRIPT_NAME="benchmark.py"

export NAIVE_FLAG="ENABLE_NAIVE_IMPLEMENTATION"
export PERF_FLAG="LHF_ENABLE_PERFORMANCE_METRICS"
export DEBUG_FLAG="LHF_ENABLE_DEBUG"
export VERIFY_DISABLE_FLAG="DISABLE_VERIFICATION"
export OUTPUT_FLAG="ENABLE_OUTPUT"

export CC=gcc
export CXX=g++
export PYTHON=python3
export TIME_EXEC="$(which time)"

# Below are likely the variables of interest.

export TEST_MAX_INT="10000"
export TEST_MAX_SET_SIZE="200"
export TEST_CORPUS_SIZE="300"
export TEST_DISTRIBUTION="uniform"