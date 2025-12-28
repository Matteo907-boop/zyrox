cmake_minimum_required(VERSION 3.16)

set(QUICKJS_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(QUICKJS_SRC  ${QUICKJS_ROOT})
set(QUICKJS_OUT  ${CMAKE_SOURCE_DIR}/deps/build)

file(READ "${QUICKJS_ROOT}/VERSION" QUICKJS_VERSION_RAW)
string(STRIP "${QUICKJS_VERSION_RAW}" QUICKJS_VERSION)

add_library(quickjs STATIC
    ${QUICKJS_SRC}/quickjs.c
    ${QUICKJS_SRC}/libregexp.c
    ${QUICKJS_SRC}/libunicode.c
    ${QUICKJS_SRC}/cutils.c
    ${QUICKJS_SRC}/dtoa.c
)

target_include_directories(quickjs PRIVATE ${QUICKJS_SRC})

target_compile_definitions(quickjs PRIVATE
    CONFIG_BIGNUM
    CONFIG_VERSION=\"${QUICKJS_VERSION}\"
)

target_compile_options(quickjs PRIVATE
    -O2
    -Wall
    -Wno-array-bounds
    -fPIC
)

set_target_properties(quickjs PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY ${QUICKJS_OUT}
)
