# ==============================================================================
# PORTH-IO: SOVEREIGN BUILD ENVIRONMENT (Golden Image)
# ==============================================================================

FROM ubuntu:24.04 AS base

# 1. Base Configuration & Determinism Guarantees
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC
ENV LC_ALL=C.UTF-8

# 2. Install Immutable System Toolchain & Kernel Primitives
RUN apt-get update && apt-get install -y --no-install-recommends \
    lsb-release \
    software-properties-common \
    wget \
    curl \
    gnupg \
    git \
    cmake \
    ninja-build \
    ccache \
    pkg-config \
    libelf-dev \
    libbpf-dev \
    libxdp-dev \
    linux-headers-generic \
    python3 \
    python3-pip \
    zip \
    unzip \
    tar \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 3. LLVM 20 (Sovereign Compiler Stack)
RUN wget -qO- https://apt.llvm.org/llvm.sh | bash -s -- 20 && \
    apt-get clean && rm -rf /var/lib/apt/lists/* && \
    ln -sf /usr/bin/clang-20 /usr/bin/clang && \
    ln -sf /usr/bin/clang++-20 /usr/bin/clang++ && \
    ln -sf /usr/bin/clang-format-20 /usr/bin/clang-format && \
    ln -sf /usr/bin/clang-tidy-20 /usr/bin/clang-tidy && \
    ln -sf /usr/bin/lldb-20 /usr/bin/lldb

# 4. Bulletproof bpftool (Kernel-Agnostic)
RUN curl -sSfL https://github.com/libbpf/bpftool/releases/download/v7.3.0/bpftool-v7.3.0-amd64.tar.gz | tar xz && \
    mv bpftool /usr/local/bin/bpftool && \
    chmod +x /usr/local/bin/bpftool

# 5. Pre-Baked Vcpkg (Locked to Official 2024.02.14 Release)
ENV VCPKG_ROOT=/opt/vcpkg
ENV VCPKG_FORCE_SYSTEM_BINARIES=1
RUN git clone https://github.com/microsoft/vcpkg.git ${VCPKG_ROOT} && \
    cd ${VCPKG_ROOT} && \
    git checkout 2024.02.14 && \
    ./bootstrap-vcpkg.sh -disableMetrics && \
    chmod -R 777 ${VCPKG_ROOT}

# 6. Pre-Baked Compliance Tooling (Pinned for Determinism)
RUN curl -sSfL https://raw.githubusercontent.com/anchore/syft/main/install.sh | sh -s -- -b /usr/local/bin v1.42.0 && \
    curl -sSfL https://raw.githubusercontent.com/anchore/grype/main/install.sh | sh -s -- -b /usr/local/bin v0.108.0

# 7. Environment Variables & Optimization
ENV CC=clang
ENV CXX=clang++
ENV CMAKE_GENERATOR=Ninja
ENV CCACHE_DIR=/ccache
ENV PORTH_ROOT=/porth-io

RUN mkdir -p /ccache && chmod 777 /ccache

WORKDIR ${PORTH_ROOT}

LABEL maintainer="Porth-IO Engineering"
LABEL description="Hermetic Build Container for the Sovereign Logic Layer."
LABEL org.opencontainers.image.source="https://github.com/Porth-IO/porth-core"

CMD ["/bin/bash"]