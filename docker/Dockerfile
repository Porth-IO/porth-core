# Porth-IO: Sovereign Development Environment
# ---------------------------------------------------------
# This Dockerfile provides a deterministic, "Immaculate" 
# environment based on Ubuntu 24.04 and LLVM 20.

FROM ubuntu:24.04 AS base

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive

# 1. Install System Prerequisites
RUN apt-get update && apt-get install -y \
    lsb-release \
    wget \
    software-properties-common \
    gnupg \
    cmake \
    ninja-build \
    git \
    python3 \
    python3-pip \
    curl \
    libbenchmark-dev \
    && rm -rf /var/lib/apt/lists/*

# 2. Install LLVM 20 (Clang, Clang-Tidy, Clang-Format)
# This matches the toolchain used in the Sovereign CI pipeline.
RUN wget https://apt.llvm.org/llvm.sh && \
    chmod +x llvm.sh && \
    ./llvm.sh 20 && \
    rm llvm.sh

# 3. Set LLVM 20 as the Default Toolchain
RUN ln -sf /usr/bin/clang-20 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-20 /usr/bin/clang++ && \
    ln -sf /usr/bin/clang-format-20 /usr/bin/clang-format && \
    ln -sf /usr/bin/clang-tidy-20 /usr/bin/clang-tidy

# 4. Environment Variables for Build Consistency
ENV CC=clang
ENV CXX=clang++
ENV PORTH_ROOT=/porth-io

WORKDIR ${PORTH_ROOT}

# 5. Metadata for the Sovereign Logic Layer
LABEL maintainer="Porth-IO Contributors"
LABEL description="Deterministic build environment for Compound Semiconductor Logic."

# Default command launches a shell
CMD ["/bin/bash"]