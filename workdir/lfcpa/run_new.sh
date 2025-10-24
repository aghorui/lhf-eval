set -o errexit
set -o nounset
set -x

LHF_SOURCE=$(realpath ../lhf)
PROJECT_ROOT=$(pwd)

# Install LHF
cd "$LHF_SOURCE"
mkdir -p "build"
cd "build"
rm -rf *
cmake .. -DENABLE_EXAMPLES=0
make -j4
make install

# Install SLIM
cd "$PROJECT_ROOT/SLIM"
mkdir -p "build"
cd "build"
rm -rf *
cmake .. -DDISABLE_IGNORE_EFFECT=1
make -j4
make install

# Run VASCO-LFCPA
cd "$PROJECT_ROOT/VASCO-LFCPA"
mkdir -p build
./run.sh $@

set +x