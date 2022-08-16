// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Runtime CPU detection for Aarch64
//
// Based on code from the Botan project:
//
// Copyright (C) 2009,2010,2013,2017,2020 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include "cpuid.hpp"

#if defined(INFECTIOUS_TARGET_ARCH_IS_ARM64)

#include "os_utils.hpp"

namespace infectious {

uint64_t CPUID::CPUID_Data::detect_cpu_features() {
	uint64_t detected_features = 0;

#if defined(INFECTIOUS_TARGET_OS_HAS_GETAUXVAL) || defined(INFECTIOUS_TARGET_OS_HAS_ELF_AUX_INFO)
	/*
	On systems with getauxval these bits should normally be defined
	in bits/auxv.h but some buggy? glibc installs seem to miss them.
	These following values are all fixed, for the Linux ELF format,
	so we just hardcode them in ARM_hwcap_bit enum.
	*/

	enum ARM_hwcap_bit {
		NEON_bit  = (1 << 1),

		ARCH_hwcap = 16, // AT_HWCAP
	};

	const unsigned long hwcap = OS::get_auxval(ARM_hwcap_bit::ARCH_hwcap);
	if (hwcap & ARM_hwcap_bit::NEON_bit) {
		detected_features |= CPUID::CPUID_ARM_NEON_BIT;
	}

#elif defined(INFECTIOUS_TARGET_OS_IS_IOS) || defined(INFECTIOUS_TARGET_OS_IS_MACOS)

	// All 64-bit Apple ARM chips have NEON, AES, and SHA support
	detected_features |= CPUID::CPUID_ARM_NEON_BIT;

#elif defined(INFECTIOUS_USE_GCC_INLINE_ASM)

	/*
	No getauxval API available, fall back on probe functions. We only
	bother with Aarch64 here to simplify the code and because going to
	extreme contortions to detect NEON on devices that probably don't
	support it doesn't seem worthwhile.

	NEON registers v0-v7 are caller saved in Aarch64
	*/

	auto neon_probe  = []() noexcept -> int { asm("and v0.16b, v0.16b, v0.16b"); return 1; };

	if (OS::run_cpu_instruction_probe(neon_probe) == 1) {
		detected_features |= CPUID::CPUID_ARM_NEON_BIT;
	}
#endif

	return detected_features;
}

} // namespace infectious

#endif
