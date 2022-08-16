// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Runtime CPU detection for POWER/PowerPC
//
// Based on code from the Botan project:
//
// Copyright (C) 2009,2010,2013,2017,2021 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include "cpuid.hpp"

#if defined(INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY)

#include "os_utils.hpp"

namespace infectious {

uint64_t CPUID::CPUID_Data::detect_cpu_features() {
	uint64_t detected_features = 0;

#if (defined(INFECTIOUS_TARGET_OS_HAS_GETAUXVAL) || defined(INFECTIOUS_TARGET_HAS_ELF_AUX_INFO)) && defined(INFECTIOUS_TARGET_ARCH_IS_PPC64)

	enum PPC_hwcap_bit {
		ALTIVEC_bit  = (1 << 28),

		ARCH_hwcap_altivec = 16, // AT_HWCAP
	};

	const unsigned long hwcap_altivec = OS::get_auxval(PPC_hwcap_bit::ARCH_hwcap_altivec);
	if (hwcap_altivec & PPC_hwcap_bit::ALTIVEC_bit) {
		detected_features |= CPUID::CPUID_ALTIVEC_BIT;
	}

#else

	auto vmx_probe = []() noexcept -> int { asm("vor 0, 0, 0"); return 1; };

	if (OS::run_cpu_instruction_probe(vmx_probe) == 1) {
		detected_features |= CPUID::CPUID_ALTIVEC_BIT;
	}

#endif

	return detected_features;
}

} // namespace infectious

#endif
