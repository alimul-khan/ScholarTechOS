FROM ubuntu:24.04
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential git wget cpio unzip rsync bc file patch perl python3 \
    libncurses-dev libssl-dev ca-certificates sed gzip bzip2 xz-utils \
    cmake ninja-build pkg-config && rm -rf /var/lib/apt/lists/*
RUN useradd -m builder
USER builder
WORKDIR /work
