# Copyright (c) 2021-2022 Storj Labs, Inc.
# See LICENSE for copying information.

set(CMAKE_CXX_STANDARD 20)

include(GenerateExportHeader)

set(HEADER_LIST
    "${infectious_cpp_SOURCE_DIR}/include/infectious/fec.hpp"
    "${infectious_cpp_SOURCE_DIR}/include/infectious/build_env.h"
    "${infectious_cpp_SOURCE_DIR}/src/tables.hpp"
    "${infectious_cpp_SOURCE_DIR}/src/cpuid.hpp"
    "${infectious_cpp_SOURCE_DIR}/src/simd_32.hpp"
    "${infectious_cpp_SOURCE_DIR}/src/gf_alg.hpp"
)

add_library(infectious SHARED
    addmul.cpp
    addmul_sse2.cpp
    addmul_vperm.cpp
    berlekamp_welch.cpp
    cpuid.cpp
    cpuid_aarch64.cpp
    cpuid_arm32.cpp
    cpuid_ppc.cpp
    cpuid_x86.cpp
    fec.cpp
    os_utils.cpp
    ${HEADER_LIST}
)
generate_export_header(infectious)

target_include_directories(infectious PUBLIC ${infectious_cpp_SOURCE_DIR}/include ${infectious_cpp_BINARY_DIR}/src)
