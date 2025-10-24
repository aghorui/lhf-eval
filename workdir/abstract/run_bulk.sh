#!/bin/bash
set -o xtrace
set -o errexit
set -o nounset

ENVIRONMENT_TO_USE="$(realpath environment_setup_optimistic.sh)" bash run_all_tests_closedworld.sh 2>&1 | tee output_major_1_closedworld_optimistic.txt
ENVIRONMENT_TO_USE="$(realpath environment_setup_uniform.sh)" bash run_all_tests_uniform.sh 2>&1 | tee output_major_1_uniform.txt
ENVIRONMENT_TO_USE="$(realpath environment_setup_uniform.sh)" bash run_all_tests_closedworld.sh 2>&1 | tee output_major_1_closedworld_uniform.txt

ENVIRONMENT_TO_USE="$(realpath environment_setup_optimistic.sh)" bash run_all_tests_closedworld.sh 2>&1 | tee output_major_2_closedworld_optimistic.txt
ENVIRONMENT_TO_USE="$(realpath environment_setup_uniform.sh)" bash run_all_tests_uniform.sh 2>&1 | tee output_major_2_uniform.txt
ENVIRONMENT_TO_USE="$(realpath environment_setup_uniform.sh)" bash run_all_tests_closedworld.sh 2>&1 | tee output_major_2_closedworld_uniform.txt

ENVIRONMENT_TO_USE="$(realpath environment_setup_optimistic.sh)" bash run_all_tests_closedworld.sh 2>&1 | tee output_major_3_closedworld_optimistic.txt
ENVIRONMENT_TO_USE="$(realpath environment_setup_uniform.sh)" bash run_all_tests_uniform.sh 2>&1 | tee output_major_3_uniform.txt
ENVIRONMENT_TO_USE="$(realpath environment_setup_uniform.sh)" bash run_all_tests_closedworld.sh 2>&1 | tee output_major_3_closedworld_uniform.txt