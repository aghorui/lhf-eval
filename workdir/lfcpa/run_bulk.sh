set -o errexit
set -o nounset
set -x

PROJECT_ROOT=$(pwd)
run_analysis() {
	echo "@@@@@@@@@ RUNNING FOR $1 @@@@@@@@@"
	bash ./run_new.sh "$1" --FUNPTR  2>&1 | tee "output_$1_funptr.txt"
}

run_analysis_naive() {
	echo "@@@@@@@@@ RUNNING FOR $1 @@@@@@@@@"
	bash ./run_new.sh "$1" --FUNPTR --NAIVE_MODE  2>&1 | tee "output_$1_funptr_naive.txt"
}

# run_analysis "lbm"
# run_analysis_naive "lbm"
# run_analysis "mcf"
# run_analysis_naive "mcf"
# run_analysis "libquantum"
# run_analysis_naive "libquantum"
# run_analysis "bzip2"
# run_analysis_naive "bzip2"
# run_analysis "sjeng_withstoreins"
# run_analysis_naive "sjeng_withstoreins"
# run_analysis "hmmer"
# run_analysis_naive "hmmer"

run_analysis "bzip2-stock"
run_analysis_naive "bzip2-stock"
run_analysis "nano"
run_analysis_naive "nano"

set +x