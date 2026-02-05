FROM python:3.10-slim

ENV TZ=UTC
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
 && rm -rf /var/lib/apt/lists/*

# Make LLVM 20 default
RUN update-alternatives --install /usr/bin/clang clang /usr/bin/clang-20 100 && \
    update-alternatives --install /usr/bin/clang++ clang++ /usr/bin/clang++-20 100 && \
    update-alternatives --install /usr/bin/llvm-profdata llvm-profdata /usr/bin/llvm-profdata-20 100 && \
    update-alternatives --install /usr/bin/llvm-cov llvm-cov /usr/bin/llvm-cov-20 100

# Set woring directory
WORKDIR /work

# Copy project files
COPY pyproject.toml ./
COPY src ./src

# Upgrade pip and install Python dependencies from pyproject.toml
RUN pip install --no-cache-dir --upgrade pip setuptools wheel && \
    pip install --no-cache-dir -e .

# Set the default command
ENTRYPOINT ["casp"]
CMD ["--help"]

