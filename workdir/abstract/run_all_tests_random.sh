#!/bin/bash
# set -o xtrace
set -o errexit
set -o nounset

TEST_ROOT="$(pwd)"

run_test_small() {
	echo "@@@@@@@@@ Test Series $1 @@@@@@@@"
	cd "$TEST_ROOT/$1"

	echo "@@@@ Running test 5000 @@@@"
	bash test.sh run 5000
	echo "@@@@ Running test 5000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 10000 @@@@"
	bash test.sh run 10000
	echo "@@@@ Running test 10000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 15000 @@@@"
	bash test.sh run 15000
	echo "@@@@ Running test 15000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 20000 @@@@"
	bash test.sh run 20000
	echo "@@@@ Running test 20000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 25000 @@@@"
	bash test.sh run 25000
	echo "@@@@ Running test 25000 naive @@@@"
	bash test.sh run nochange naive

	echo "@@@@ Running test 30000 @@@@"
	bash test.sh run 30000
	echo "@@@@ Running test 30000 naive @@@@"
	bash test.sh run nochange naive

	bash test.sh clean
}

run_test_small "random";
run_test_small "random_pointsto";