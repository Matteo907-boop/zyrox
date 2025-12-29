#!/usr/bin/env bash
set -e
 
sudo docker build -t zyrox-builder -f Dockerfile .

mkdir -p ./build
 
sudo docker run --rm \
    -v "$(pwd)":/zyrox \
    -w /zyrox \
    zyrox-builder \
    bash -c "cmake -Bbuild -S. \
        -DCMAKE_C_COMPILER=/usr/bin/clang-18 \
        -DCMAKE_CXX_COMPILER=/usr/bin/clang++-18 \
      && cmake --build build --parallel $(nproc)"

echo "✅ libzyrox.so built at ./build/libzyrox.so"
