// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Runtime CPU detection for 32-bit ARM
//
// Based on code from the Botan project:
//
// Copyright (C) 2009,2010,2013,2017 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include "cpuid.hpp"

#if defined(INFECTIOUS_TARGET_ARCH_IS_ARM32)

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
		NEON_bit  = (1 << 12),

		ARCH_hwcap_neon   = 16, // AT_HWCAP
	};

	const unsigned long hwcap_neon = OS::get_auxval(ARM_hwcap_bit::ARCH_hwcap_neon);
	if (hwcap_neon & ARM_hwcap_bit::NEON_bit) {
		detected_features |= CPUID::CPUID_ARM_NEON_BIT;
	}
#endif

	return detected_features;
}

} // namespace infectious

#endif
