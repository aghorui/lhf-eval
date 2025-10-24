#!/bin/bash
# set -o xtrace
set -o errexit
set -o nounset

. $ENVIRONMENT_TO_USE

if [ "$1" = "clean" ]; then
	rm -f $TEST_EXEC_FILE_NAME;
	rm -f $TEST_DATA_FILE_NAME;
	exit 0;
fi

if [ "$1" = "run" ]; then
	OPER_COUNT=""
	if [ -z "${2+x}" ]; then
		echo "Usage: $0 run <number of operations | 'nochange'>";
		exit 1;
	else
		OPER_COUNT="$2";
	fi

	ADD_NAIVE_FLAG=""
	if [ ! -z "${3+x}" ] && [ "$3" == "naive" ]; then
		echo "Running with naive implementation.";
		ADD_NAIVE_FLAG="-D$NAIVE_FLAG"
	fi

	set -o xtrace

	if [ ! "$OPER_COUNT" = "nochange" ]; then
		"$TIME_EXEC" -f 'Python Wallclock Time: %e, Max RSS (KB): %M' \
		"$PYTHON" "$TEST_BENCHMARK_SCRIPT_NAME"\
			--count "$2" \
			--maxsize "$TEST_MAX_SET_SIZE" \
			--maxval "$TEST_MAX_INT" \
			--dist "$TEST_DISTRIBUTION" \
			--corpussize "$TEST_CORPUS_SIZE" \
			"$TEST_DATA_FILE_NAME"
	fi

	"$CXX" "$TEST_SRC_FILE_NAME" -o "$TEST_EXEC_FILE_NAME" \
		-D"$VERIFY_DISABLE_FLAG" \
		$ADD_NAIVE_FLAG \
		-I"$TEST_LHF_INCLUDE_DIR" \
		-I"$TEST_INCLUDE_DIR";

	"$TIME_EXEC" -f 'Wallclock Time: %e, Max RSS (KB): %M' \
		"./$TEST_EXEC_FILE_NAME" "$TEST_DATA_FILE_NAME"

	set +o xtrace
fi

if [ "$1" = "run-verify" ]; then
	OPER_COUNT=""
	if [ -z "${2+x}" ]; then
		echo "Usage: $0 run <number of operations | 'nochange'>";
		exit 1;
	else
		OPER_COUNT="$2";
	fi

	ADD_NAIVE_FLAG=""
	if [ ! -z "${3+x}" ] && [ "$3" == "naive" ]; then
		echo "Running with naive implementation.";
		ADD_NAIVE_FLAG="-D$NAIVE_FLAG"
	fi

	set -o xtrace

	if [ ! "$OPER_COUNT" = "nochange" ]; then
		"$PYTHON" "$TEST_BENCHMARK_SCRIPT_NAME"\
			--count "$2" \
			--maxsize "$TEST_MAX_SET_SIZE" \
			--maxval "$TEST_MAX_INT" \
			--dist "$TEST_DISTRIBUTION" \
			--corpussize "$TEST_CORPUS_SIZE" \
			"$TEST_DATA_FILE_NAME"
	fi

	"$CXX" "$TEST_SRC_FILE_NAME" -o "$TEST_EXEC_FILE_NAME" \
		-D"$DEBUG_FLAG" \
		-D"$OUTPUT_FLAG" \
		-D"$PERF_FLAG" \
		$ADD_NAIVE_FLAG \
		-I"$TEST_LHF_INCLUDE_DIR" \
		-I"$TEST_INCLUDE_DIR";

	"$TIME_EXEC" -f 'Wallclock Time: %e, Max RSS (KB): %M' \
		"./$TEST_EXEC_FILE_NAME" "$TEST_DATA_FILE_NAME"

	set +o xtrace
fi
