# Copyright (c) 2021-2022 Storj Labs, Inc.
# See LICENSE for copying information.

set(CMAKE_CXX_STANDARD 20)

find_package(PkgConfig)
pkg_search_module(GTEST REQUIRED gtest)

set(HEADER_LIST
    "${infectious_cpp_SOURCE_DIR}/tests/random_env.hpp"
)

add_executable(infectious-test
    berlekamp_welch_test.cpp
    fec_test.cpp
    gf_alg_test.cpp
    zfec_compat_test.cpp
    test_main.cpp
    ${HEADER_LIST})

target_link_libraries(infectious-test infectious ${GTEST_LDFLAGS})
target_compile_options(infectious-test PUBLIC ${GTEST_CFLAGS})
target_include_directories(infectious-test PRIVATE ${infectious_cpp_SOURCE_DIR}/src)

include(GoogleTest)
gtest_discover_tests(infectious-test)
