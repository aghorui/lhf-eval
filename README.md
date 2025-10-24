**NOTE 1: The entire docker image (440 MB) is not supplied in this version for
cutting down the repository size. Please build it yourself using the
instructions provided.**

**NOTE 2: A certain part of the experiments requires SUPERUSER ACCESS for proper
functioning. While care has been taken to ensure that the system is not damaged,
we strongly recommend running all experiments in a controlled, isolated
environment, and/or within the supplied docker container. While this can be
circumvented, we have made minimal modifications to the build scripts to
maintain stability of the existing system.**

# LHF Evaluation Artifact

This software package is meant to demonstrate the validity of the claims made by
the accompanying publication for [LatticeHashForest](https://aghorui.github.io/lhf/).

All of the tools presented here should be runnable on a standard, recent Linux
machine, however we can only confirm them to be in a working state in the
experimental setup described below. Further explanations and instructions for
each experiment performed are given later in the document.

## Quick Start

If you are only interested in replicating the results from the paper, do the
following:

* Start a container from the [Docker image](./lhf-artifact-docker.tar.gz):
  `python tool.sh shell` (or supply your own custom commands).

* Mount [`workdir`](./workdir) in the container to some point (say `/workdir`).

* Run `bash /workdir/abstract/run_bulk.sh` for `Claim 1`  and `Claim 2` results.

* Run `bash /workdir/lfcpa/run_bulk.sh` for the partial `Claim 3` results.

Please note that running all of the tests may require at least **24-48 hours of
time** and about **64 GB of RAM** (upper limit) on a recent computer with
**Ubuntu 24.04** and **Docker** installed. Please ensure you have a machine that
matches these specifications.

## Experimental Setup

This package has been tested on the following machine configuration:
* A university-maintained data center with 2 10-core CPUs, and 256 GB of RAM.
* A virtual machine hosted on the data center with:
  * Ubuntu 22.04,
  * 4 vCPUs,
  * 64 GB of RAM, and
  * 75 GB of disk space.
* GCC 11.4.0, with no optimization flags supplied.
* LLVM-14.

The specification of the hardware, except for the availability of high amount
of RAM, is not important for replicating the trends of each experiment. Hence,
since this setup may be difficult to replicate, any recent machine with a large
amount of RAM should be sufficient.

### Docker Image

We provide a Docker image that will replicate the software testing environment
on a machine that supports docker. The entire docker image is provided in
['lhf-artifact-docker.tar.gz'](./lhf-artifact-docker.tar.gz).

The equivalent Dockerfile is provided in [`./Dockerfile`](./Dockerfile) if you
would like to build the image yourself:

```
docker build -t "lhf-artifact-docker" .
docker save lhf-artifact-docker | gzip > lhf-artifact-docker.tar.gz
```

Please note that the image is based on Ubuntu 24.04 rather than Ubuntu 22.04. We
don't believe the OS version and accompanying userland applications impart a
difference on the experiments.

## Running the Experiments

The experiment files are provided separately from the Docker image in the folder
[`workdir`](./workdir), and is bind-mounted to the container. This allows a user
to freely explore the files without having to go through Docker.

A Python 3 script called `tool.py` is supplied which can be used to
automatically run the required docker commands for setting up the environment.
To start the container, run the following command:

```
python3 tool.py shell
```

For the majority of the time, you would likely want this to be ran as a
background process on which you can attach a shell to check on progress. To do
so, please use the `--background` flag:

```
python3 tool.py shell --background
```

To attach to the container, use:

```
python3 tool.py attach
```

Finally, clean up after the container is stopped, or to start a new container,
you will have to run the `clean` command:

```
python3 tool.py clean
```

If you do not want to use the script, you must do the following:

* Register the image `lhf-artifact-docker.tar.gz` into Docker.
* Start a new container and bind-mount `./workdir` to it and start a shell.
* Ensure that the session can be detached and run in the background (or as per
  your preferences).
* Change the directory to wherever `workdir` has been put.

## Supplied LHF Implementation

A known, working version of the LHF library is supplied in
[`workdir/lhf`](./workdir/abstract). The experiments should ideally work with
any future LHF version, but a copy is provided here as contingency.

## Claims Made by the Paper

Please refer to the paper, or the [preprint version](https://arxiv.org/abs/2510.18496)
for the claims made in it. Nonetheless, here is a rough summary:

* `Claim 1`: LHF performs *worse* compared to a naive implementation when there
  is no redundancy in data or operations, and there is no constraint on how much
  new data the system can receive.

* `Claim 2`: LHF performs *better* compared to a naive implementation when there
  *is* redundancy in data and operations, and all data in the system is
  synthesized from an initial corpus of data.

* `Claim 3`: LHF performs better compared to a naive implementation when applied
  to VASCO-LFCPA.

By "better performance", we are referring to reduction in both execution time
and memory usage.

## Demonstration of `Claim 1` and `Claim 2`

The implementation of these experiments is present in
[`workdir/abstract`](./workdir/abstract). The contents of the folder are
structured as follows:

**Preliminiaries:**

 * `include/`:

   Contains the naive and the LHF-based implementations of the integer set data
   structure. **This is a possible point of interest.** The two implementations
   are in separate files and are switched between with a preprocessor definition.

 * `past_outputs/`:

   Contains outputs generated from our experimental runs. Also includes
   accompanying scripts for extracting output from the logs and Jupyter
   notebooks for presenting the data.

**Experiments:**

 * `random/`:

   Generates experimental results for `Claim 1` in the pointee set case.

 * `random_pointsto/`:

   Generates experimental results for `Claim 1` in the points-to set case.

 * `closed_world/`:

   Generates experimental results for `Claim 2` in the pointee-set case.
   Pessimistic and optimistic cases are switched between by the script files
   specified below.

 * `closed_world_pointsto/`

   Generates experimental results for `Claim 2` in the pointee-set case.
   Pessimistic and optimistic cases are switched between by the script files
   specified below.

Each experiment is structured with the following files.

* `benchmark.py`

  Generates the payload for the test program. Several arguments for controlling
  the experiment's parameters can be supplied to this script. Short descriptions
  for each are provided in `python3 benchmark.py --help`

* `test.cpp`:

  Contains the program that will process the payload generated by `test.cpp`.

* `test.sh`:

  Compiles and executes `test.cpp`. The following parameters can be supplied:
  `$ bash test.sh ('run'|'run-verify') (number of operations|'nochange'>)`.

  * `run-verify` executes the test while having the verification routines
    enabled. This is useful for checking whether the program is actually
    providing the correct output.

  * `run` disables all verification checks within the program and runs it. This
    is used for generating the times for benchmarking.

  * Supplying `nochange` as the second argument will not invoke `benchmark.py`
    for regenerating the payload, and will instead make the program use the
    existing one. Otherwise, an integer must be supplied denoting the number of
    operations required in the payload.

These files use a variety of preprocessor definitions and environment variables
to set up the experiments. It is important to pay special attention to them if
one wants to perform an examination of these files.

**Script Files:**

 * `environment_setup_optimistic.sh`:

   Environment and variable setup for `Claim 2` in the optimistic case. Can be
   modified to change experiment parmeters.

 * `environment_setup_uniform.sh`:

   Environment and variable setup for `Claim 1` and `Claim 2` in the uniformly
   random cases. Can me modified to change experiment parmeters.

 * `run_all_tests_closedworld.sh`:

   Runs the test series for `Claim 2` for all data points used in the paper.
   The environment variable `ENVIRONMENT_TO_USE` must be set to one of the
   above mentioned `environment_*.sh` scripts.

 * `run_all_tests_random.sh`:

   Runs the test series for `Claim 1` for all data points used in the paper.
   The environment variable `ENVIRONMENT_TO_USE` must be set to one of the
   above mentioned `environment_*.sh` scripts.

 * `run_bulk.sh`:

   Runs all of the tests provided in the paper, and logs the output of each
   invocation. These outputs can be used to generate the data for the graphs
   presented in the paper.

**To run the experiments:** Run `bash run_bulk.sh`. Output logs will be
generated in the same directory.

**Expected output:** The logs should suggest that `Claim 1` and `Claim 2`
hold true.

**Expected time:** 24-30 hours for entire run. Individual times vary.

## Demonstration of `Claim 3`

Since the SPEC CPU 2006 Benchmarks cannot be supplied separately, we supply
alternate data demonstrating, in support of the claim, the same trends as shown
in the paper. They are as follows:

* `nano`: GNU Nano text editor, compiled to a single LLVM IR file.
* `bzip2-stock`: The Bzip2 stock source code compiled to a single LLVM IR file.
  This is different from the SPEC CPU 2006 benchmark version.

Both of these programs have been compiled with the
[`wllvm`](https://github.com/travitch/whole-program-llvm) tool. The build
scripts for each are provided in [`sources/`](./sources).

The experiment files for this are contained in
[`workdir/lfcpa`](./workdir/abstract). The contents are as follows:

* `past_outputs`:

  Contains outputs generated from our experimental runs.

* `SLIM`:

  Implementation of the `SLIM` LLVM helper library.

* `VASCO-LFCPA`

  Implementation of VASCO-LFCPA. The naive and LHF implementations of the data
  representations are present in:
  * `VASCO-LFCPA/include/CommonNaive.h`, and
  * `VASCO-LFCPA/include/Common.h` respectively.
  **This is a possible point of interest.**

* `run_new.sh`

  Sets up the experiment and runs it for the specified IR file in the
  `workdir/lfcpa/VASCO-LFCPA/samples` folder. Note that this script requires
  **superuser access** for installing SLIM onto the system for VASCO-LFCPA. This
  can be circumvented by either installing SLIM manually and editing the script
  to remove the installation routine, or updating the CMake build scripts in
  VASCO-LFCPA such that it links against a locally-available build of SLIM.

  For the sake of simplicity and not disturbing the existing configurations too
  much, we have left this part of the process as-is.

* `run_bulk.sh`

  Runs all available experiments. Invokes `run_new.sh`.

**To run the experiments:** Run `bash run_bulk.sh`. Output logs will be
generated in the same directory.

**Expected output:** The logs should suggest that `Claim 3` holds true.

**Expected time:** 24-48 hours for entire run. Individual times vary.

### Notes on VASCO-LFCPA

The current C++ implementation of VASCO-LFCPA is highly unstable, experimental
and being actively worked on. It is known to only work with SPEC CPU 2006
benchmarks converted to LLVM IR and low complexity C programs. The results
generated by VASCO LFCPA are **not** guaranteed to be sound.

The implementation may suffer segmentation faults, abort unexpectedly, or not
terminate at all. This is dependent on the IR supplied to the system. The
output may not be deterministic either. This is because of issues with the
worklist management system for VASCO. However, the output can be expected to
consistently be within a given range of iterations, generated contexts and
values.

The general trends of VASCO-LFCPA can still be observed with other IR files
provided that they actually manage to run on VASCO-LFCPA without fail. We
provide the above two sample IR files to demonstrate this.

To actually receive analysis output from VASCO-LFCPA, the flag `--SHOW_OUTPUT`
must be passed to `run_new.sh`. Please see `run_bulk.sh` for invocation
examples.

## Other Features in the LHF Project

If you would like to experiment with LHF itself, we suggest the following lines
of inquiry.

* **Cache-like Hits and Misses**:

  This one is mentioned in the paper, and can be enabled with the compile-time
  switch `LHF_ENABLED_PERFORMANCE_STATISTICS`. One may obtain outputs that look
  like the following:

```
Performance Profile:
unions
      Hits       : 932
      Equal Hits : 608
      Subset Hits: 0
      Empty Hits : 1732
      Cold Misses: 99
      Edge Misses: 0

differences
      Hits       : 808
      Equal Hits : 731
      Subset Hits: 0
      Empty Hits : 1385
      Cold Misses: 57
      Edge Misses: 50

intersections
      Hits       : 1322
      Equal Hits : 465
      Subset Hits: 0
      Empty Hits : 1702
      Cold Misses: 53
      Edge Misses: 56

property_sets
      Hits       : 10338
      Equal Hits : 0
      Subset Hits: 0
      Empty Hits : 0
      Cold Misses: 278
      Edge Misses: 0


Profiler Statistics:
    'register_set': 188.121 ms
    'set_difference': 3.967 ms
    'set_intersection': 3.913 ms
    'set_union': 5.156 ms
    'store_subset': 0.328 ms
```

* **Parallelism and Set Eviction**:

  This can be enabled with flags `LHF_ENABLE_PARALLEL` and
  `LHF_ENABLE_EVICTION`. Please do keep in mind that these are nascent features
  and have not yet been substantially tested. These may contain bugs.

* **Unit Tests**:

  There are various unit tests written for LHF using the GTest framework. These
  are present in the [`src/tests/`](./workdir/lhf/src/tests/) directory of the
  LHF source tree. These may be useful for illustrations of constraints,
  edge-cases, and usage demonstrations.

We recommend going through the sourcecode and documentation for a fuller
picture, and the CMake build script for a full list of available compile-time
flags.

## License

Copyright (C) 2025 Anamitra Ghorui and Aditi Raste

This program is free software: you can redistribute it and/or modify it under
the terms of the GNU Affero General Public License as published by the Free
Software Foundation, either version 3 of the License, or (at your option) any
later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU Affero General Public License for more details.

You should have received a copy of the GNU Affero General Public License along
with this program. If not, see <https://www.gnu.org/licenses/>.

Please refer to [LICENSE](./LICENSE) for the full license text.