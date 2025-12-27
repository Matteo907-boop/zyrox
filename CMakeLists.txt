cmake_minimum_required(VERSION 3.10)
project(zyrox DESCRIPTION "Zyrox llvm pass")

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

set(CMAKE_CXX_STANDARD 20)

find_package(LLVM REQUIRED CONFIG)

find_package(LLVM 18.1.3 CONFIG REQUIRED)

include_directories(
        ${LLVM_INCLUDE_DIRS}
        include
)

add_library(zyrox SHARED
        src/ZyroxPlugin.cpp

        src/core/ZyroxCore.cpp
        src/core/ZyroxPassOptions.cpp
        src/core/ZyroxMetaData.cpp

        src/quickjs/QuickRt.cpp
        src/quickjs/QuickConfig.cpp

        src/util/BasicBlockUtils.cpp
        src/util/FunctionUtils.cpp
        src/util/OpaqueTransformer.cpp
        src/util/ModuleUtils.cpp
        src/util/HashUtils.cpp
        src/util/CryptoUtils.cpp
        src/util/Random.cpp

        src/Passes/BasicBlockSplitter.cpp
        src/Passes/MBASub.cpp
        src/Passes/IndirectBranch.cpp
        src/Passes/ControlFlowFlattening.cpp
        src/Passes/SimpleIndirectBranch.cpp
        src/Passes/StringEncryption.cpp
)

target_link_libraries(zyrox PRIVATE ${CMAKE_SOURCE_DIR}/libs/libquickjs.a)
