FROM ubuntu:24.04

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
        apt-get install -y --no-install-recommends \
            build-essential gcc gdb zlib1g zlib1g-dev \
            tmux htop valgrind cmake time \
            llvm-14 llvm-14-dev clang-14 \
            python3 python3-pip pipx && \
    pipx install wllvm

WORKDIR /workdir