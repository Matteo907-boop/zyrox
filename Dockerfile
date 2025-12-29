FROM ubuntu:24.04 AS llvm-base
ENV DEBIAN_FRONTEND=noninteractive

RUN apt update && \
  apt install -y \
  clang-18 \
  llvm-18 \
  llvm-18-dev \
  cmake \
  make \
  && apt clean && rm -rf /var/lib/apt/lists/*

WORKDIR /zyrox
