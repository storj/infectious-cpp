// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Runtime CPU detection for x86
//
// Based on code from the Botan project:
//
// Copyright (C) 2009,2010,2013,2017 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include "cpuid.hpp"

#if defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)

#if defined(INFECTIOUS_BUILD_COMPILER_IS_MSVC)
# include <intrin.h>
#elif defined(INFECTIOUS_BUILD_COMPILER_IS_INTEL)
# include <ia32intrin.h>
#elif defined(INFECTIOUS_BUILD_COMPILER_IS_GCC) || defined(INFECTIOUS_BUILD_COMPILER_IS_CLANG)
# include <cpuid.h>
#endif

#endif

namespace infectious {

#if defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)

namespace {

void invoke_cpuid(uint32_t type, uint32_t out[4]) {
#if defined(INFECTIOUS_BUILD_COMPILER_IS_MSVC) || defined(INFECTIOUS_BUILD_COMPILER_IS_INTEL)
	__cpuid((int*)out, type);

#elif defined(INFECTIOUS_BUILD_COMPILER_IS_GCC) || defined(INFECTIOUS_BUILD_COMPILER_IS_CLANG)
	__get_cpuid(type, out, out+1, out+2, out+3);

#elif defined(INFECTIOUS_USE_GCC_INLINE_ASM)
	asm("cpuid\n\t"
		: "=a" (out[0]), "=b" (out[1]), "=c" (out[2]), "=d" (out[3])
		: "0" (type));

#else
	#warning "No way of calling x86 cpuid instruction for this compiler"
	clear_mem(out, 4);
#endif
}

} // namespace

uint64_t CPUID::CPUID_Data::detect_cpu_features() {
	uint64_t features_detected = 0;
	uint32_t cpuid[4] = { 0 };

	// CPUID 0: vendor identification, max sublevel
	invoke_cpuid(0, cpuid);
	const uint32_t max_supported_sublevel = cpuid[0];

	if (max_supported_sublevel >= 1) {
		// CPUID 1: feature bits
		invoke_cpuid(1, cpuid);
		const uint64_t flags0 = (static_cast<uint64_t>(cpuid[2]) << 32) | cpuid[3];

		enum x86_CPUID_1_bits : uint64_t {
			SSE2  = (1ULL << 26),
			SSSE3 = (1ULL << 41),
		};

		if (flags0 & x86_CPUID_1_bits::SSE2)  { features_detected |= CPUID::CPUID_SSE2_BIT; }
		if (flags0 & x86_CPUID_1_bits::SSSE3) { features_detected |= CPUID::CPUID_SSSE3_BIT; }
	}

	/*
	If we don't have access to CPUID, we can still safely assume that
	any x86-64 processor has SSE2.
	*/
#if defined(INFECTIOUS_TARGET_ARCH_IS_X86_64)
	if (features_detected == 0) {
		features_detected |= CPUID::CPUID_SSE2_BIT;
	}
#endif

	return features_detected;
}

#endif

} // namespace infectious
