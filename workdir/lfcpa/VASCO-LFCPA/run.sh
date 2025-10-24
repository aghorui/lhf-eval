#!/bin/bash

CC="$LLVM_HOME/bin/clang-14"
OPT="$LLVM_HOME/bin/opt-14"
DRIVER="./build/iplfcpa"

ARGS=`python3 ./HandleArgs.py ${@:2}`

[ "$ARGS" ] || exit 1

if [[ ${#ARGS} -gt 200 ]]; then
    python3 ./HandleArgs.py $@
    exit 1
fi

# convert string to array
ARG=(`echo ${ARGS}`);

FOO_VAR=''


for (( Idx=0; Idx<${#ARG[@]}; Idx++ ))
do
	echo "${ARG[$Idx]}"
	FOO_VAR="${FOO_VAR}""-D"${ARG[$Idx]}';'
	
done


FOO_VAR=`echo $FOO_VAR | sed -e 's/;$//g'`
echo "$FOO_VAR"
export FOO_VAR

mkdir -p build
cd build
make clean
cmake ..
make
cd ..

FILENAME=$(basename $1 .c)

echo -e "=======================Generate LL Files using CLANG======================="

rm -f *.ll
$OPT -S -instnamer -mem2reg -mergereturn ./samples/$FILENAME.ll -o $FILENAME.ll

echo -e "==============================Running Analysis================================\n"

TIME_EXEC=/usr/bin/time
"$TIME_EXEC" -f 'Wallclock Time: %e, Max RSS (KB): %M' $DRIVER $FILENAME.ll

echo -e "\n"
echo -e "==============================Analysis Over================================"
