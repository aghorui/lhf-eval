#!/bin/bash
# set -o xtrace
set -o errexit
set -o nounset

TEST_ROOT="$(pwd)"

run_test() {
	echo "@@@@@@@@@ Test Series $1 @@@@@@@@"
	cd "$TEST_ROOT/$1"

	echo "@@@@ Running test 100000 @@@@"
	bash test.sh run 100000
	echo "@@@@ Running test 100000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 200000 @@@@"
	bash test.sh run 200000
	echo "@@@@ Running test 200000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 500000 @@@@"
	bash test.sh run 500000
	echo "@@@@ Running test 500000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 1000000 @@@@"
	bash test.sh run 1000000
	echo "@@@@ Running test 1000000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 2000000 @@@@"
	bash test.sh run 2000000
	echo "@@@@ Running test 2000000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 4000000 @@@@"
	bash test.sh run 4000000
	echo "@@@@ Running test 4000000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 6000000 @@@@"
	bash test.sh run 6000000
	echo "@@@@ Running test 6000000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 8000000 @@@@"
	bash test.sh run 8000000
	echo "@@@@ Running test 8000000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 10000000 @@@@"
	bash test.sh run 10000000
	echo "@@@@ Running test 10000000 naive @@@@"
	bash test.sh run nochange naive

	bash test.sh clean
}

run_test "closed_world";
run_test "closed_world_pointsto";
