// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// OS specific utility functions
//
// Based on code from the Botan project:
//
// (C) 2015,2016,2017,2018 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#ifndef INFECTIOUS_OS_UTILS_HPP
#define INFECTIOUS_OS_UTILS_HPP

#include <functional>

#include "infectious/build_env.h"

namespace infectious::OS {

///
// Return the ELF auxiliary vector cooresponding to the given ID.
// This only makes sense on Unix-like systems and is currently
// only supported on Linux, Android, and FreeBSD.
//
// Returns zero if not supported on the current system or if
// the id provided is not known.
auto get_auxval(unsigned long id) -> unsigned long;

///
// Run a probe instruction to test for support for a CPU instruction.
// Runs in system-specific env that catches illegal instructions; this
// function always fails if the OS doesn't provide this.
// Returns value of probe_fn, if it could run.
// If error occurs, returns negative number.
// This allows probe_fn to indicate errors of its own, if it wants.
// For example the instruction might not only be only available on some
// CPUs, but also buggy on some subset of these - the probe function
// can test to make sure the instruction works properly before
// indicating that the instruction is available.
//
// @warning on Unix systems uses signal handling in a way that is not
// thread safe. It should only be called in a single-threaded context
// (ie, at static init time).
//
// If probe_fn throws an exception the result is undefined.
//
// Return codes:
// -1 illegal instruction detected
auto run_cpu_instruction_probe(const std::function<int ()>& probe_fn) -> int;

} // namespace infectious::OS

#endif // ifndef INFECTIOUS_OS_UTILS_HPP
