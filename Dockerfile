FROM ubuntu:24.04

ENV TZ=UTC
ENV DEBIAN_FRONTEND=noninteractive
ENV CMAKE_VERSION=3.28.3

# Install system dependencies for clang/LLVM and build tools
RUN apt-get update && apt-get install -y \
    ca-certificates \
    wget \
    curl \
    gnupg \
    lsb-release \
    software-properties-common \
    git \
    patch \
    build-essential \
 && rm -rf /var/lib/apt/lists/*

# Install CMake 3.28.3 
RUN wget -q https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz \
 && tar -xzf cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz \
 && mv cmake-${CMAKE_VERSION}-linux-x86_64 /opt/cmake \
 && ln -s /opt/cmake/bin/* /usr/local/bin/ \
 && rm cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz

# Install LLVM 20
RUN wget https://apt.llvm.org/llvm.sh \
 && chmod +x llvm.sh \
 && ./llvm.sh 20 \
 && rm llvm.sh

RUN apt-get update && apt-get install -y \
    clang-20 \
    clang-tools-20 \
    llvm-20 \
    llvm-20-dev \
    llvm-20-tools \
    lld-20 \
    liblld-20-dev \
 && rm -rf /var/lib/apt/lists/*

# Make LLVM 20 default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100 && \
    update-alternatives --install /usr/bin/llvm-profdata llvm-profdata /usr/bin/llvm-profdata-20 100 && \
    update-alternatives --install /usr/bin/llvm-cov llvm-cov /usr/bin/llvm-cov-20 100 && \
    update-alternatives --install /usr/bin/llvm-config llvm-config /usr/bin/llvm-config-20 100

# Set LLVM environment for CMake
ENV LLVM_DIR=/usr/lib/llvm-20/lib/cmake/llvm

# Set working directory
WORKDIR /casp

# Copy project files
COPY CMakeLists.txt ./
COPY include ./include
COPY lib ./lib
COPY examples ./examples

# Build CASP
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc) && \
    cp llvm-sprofgen /usr/local/bin/

# Verify installation
RUN llvm-sprofgen --help 2>&1 | head -5

# Set the default command
WORKDIR /casp/examples
CMD ["/bin/bash"]

