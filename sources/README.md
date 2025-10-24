IR Build Scripts
================

This directory contains sources for the `nano` and `bzip2` samples used to run
VASCO-LFCPA on. Compiled versions are already included in this repository to
use.

No copyright is claimed on either project and are provided here solely for
research and exploratory use. Licenses for each individual project are provided
in their respective source directories.

These are based on the source code fetched from the Ubuntu/Debian repositories.

## Building

The Python program `wllvm` (https://github.com/travitch/whole-program-llvm) is
required as a prerequisite. It can be installed with `pip` or `pipx`:

```
pipx install wllvm
```

Each directory has a `build.sh` file that contains all of the required commands
for the build setup. Running this should be enough to generate the LLVM IR
files.

```
bash build.sh
```

There's an additional pass that needs to be run for flattening any
`llvm::ConstExpr`s within the IR files, as VASCO-LFCPA cannot process these.
(This problem does not occur in the SPECCPU 2006 benchmark sources, possibly due
to presence of simpler expressions). This still does not make the analysis
sound, however, as it cannot process `GetElementPtr` instructions with
non-constant operands (this case also does not seem to occur in the SPECCPU
2006 benchmarks)

Compiling the pass requires CMake and LLVM-14. It may be worthwhile to go
through [LLVM's documentation on this](https://llvm.org/docs/WritingAnLLVMNewPMPass.html)
as well.

The pass can be compiled with a standard set of commands as follows.

```
cd flatten_constexpr_pass
mkdir -p build
cd build
cmake ...
make
```

And then used on an IR file as follows. The newly transformed IR file is what
should be passed to VASCO-LFCPA.

```
opt-14 \
	-load-pass-plugin="FlattenConstExprPass.so" \
	-passes="flatten-constexpr" \
	-S -o <OUTPUT_FILE_PATH> \
	<INPUT_FILE_PATH>
```