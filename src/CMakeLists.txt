# Copyright (c) 2021-2022 Storj Labs, Inc.
# See LICENSE for copying information.

set(CMAKE_CXX_STANDARD 20)

set(HEADER_LIST
    "${infectious_cpp_SOURCE_DIR}/include/infectious/fec.hpp"
    "${infectious_cpp_SOURCE_DIR}/include/infectious/slice.hpp"
    "${infectious_cpp_SOURCE_DIR}/src/tables.hpp"
    "${infectious_cpp_SOURCE_DIR}/src/gf_alg.hpp"
)

add_library(infectious SHARED
    addmul.cpp
    berlekamp_welch.cpp
    fec.cpp
    ${HEADER_LIST}
)

target_include_directories(infectious PUBLIC ${infectious_cpp_SOURCE_DIR}/include)
