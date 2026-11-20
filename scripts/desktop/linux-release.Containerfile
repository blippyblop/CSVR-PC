FROM docker.io/library/ubuntu:22.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    clang cmake ninja-build python3 pkg-config libsdl2-dev libegl1-mesa-dev \
    libgl1-mesa-dev libopenxr-dev zlib1g-dev git ca-certificates zip xz-utils zstd \
    && rm -rf /var/lib/apt/lists/*
RUN useradd -m -u 1000 builder
USER 1000:1000
