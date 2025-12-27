#!/bin/bash
set -e

NDK_SYSROOT="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/sysroot"
TOOLCHAIN_BIN="$NDK_PATH/toolchains/llvm/prebuilt/linux-x86_64/bin"
TARGET="aarch64-linux-android21"
MY_LIBCXX_DIR="$(pwd)/llvm-abis/arm64"

echo "compiling..."
$TOOLCHAIN_BIN/${TARGET}-clang -c \
  --target=$TARGET \
  --sysroot=$NDK_SYSROOT \
  -fvisibility=hidden \
  -D_LIBCPP_DISABLE_VISIBILITY_ANNOTATIONS \
  -D_LIBCPP_ABI_NAMESPACE=__1 \
  -ffunction-sections -fdata-sections -fPIC \
  -flto=full \
  main.c -o out/main.o


#  -I$MY_LIBCXX_DIR/include \
#  -I$MY_LIBCXX_DIR/include/c++/v1 \
#  -I$MY_LIBCXX_DIR/include/aarch64-linux-android \
#  -std=c++17 \

$TOOLCHAIN_BIN/${TARGET}-clang \
  --target=$TARGET \
  --sysroot=$NDK_SYSROOT \
  -nostdlib++ \
  -flto=full \
  -fuse-ld=/usr/bin/ld.lld \
  -Wl,--gc-sections \
  -Wl,--print-gc-sections \
  -O3 \
  -Wl,--load-pass-plugin=../cmake-build-debug/libzyrox.so \
  out/main.o \
  -Wl,-Bdynamic \
  -ldl -lm \
  -o out/main.so


#  -Wl,-Bstatic \
#    $MY_LIBCXX_DIR/libc++.a \
#    $MY_LIBCXX_DIR/libc++abi.a \
#    $MY_LIBCXX_DIR/libunwind.a \


#$TOOLCHAIN_BIN/${TARGET}-clang++ \
#  --target=$TARGET \
#  --sysroot=$NDK_SYSROOT \
#  -flto \
#  -Wl,--gc-sections \
#  -Wl,--version-script=hide.map \
#  -Wl,-strip-all \
#  -Wl,-S \
#  -nostdlib++ \
#  out/out.o \
#  -Wl,-Bstatic -L$MY_LIBCXX_DIR -lc++ -lc++abi -Wl,-Bdynamic \
#  -landroid -llog -lm \
#  -shared -o out/main.so
