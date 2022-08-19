// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Runtime CPU detection
//
// Based on code from the Botan project:
//
// Copyright (C) 2009,2010,2013,2017 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include <cassert>
#include <stdexcept>

#include "cpuid.hpp"

namespace infectious {

// they're not magic numbers, they're an endian check.
// NOLINTBEGIN(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)

//static
auto CPUID::CPUID_Data::runtime_check_endian() -> CPUID::Endian_Status {
	// Check runtime endian
	uint32_t endian32 = 0x01234567;
	const auto* e8 = reinterpret_cast<const uint8_t*>(&endian32);

	CPUID::Endian_Status endian = CPUID::Endian_Status::Unknown;

	if (e8[0] == 0x01 && e8[1] == 0x23 && e8[2] == 0x45 && e8[3] == 0x67) {
		endian = CPUID::Endian_Status::Big;
	} else if(e8[0] == 0x67 && e8[1] == 0x45 && e8[2] == 0x23 && e8[3] == 0x01) {
		endian = CPUID::Endian_Status::Little;
	} else {
		throw std::runtime_error("Unexpected endian at runtime, neither big nor little");
	}

	// If we were compiled with a known endian, verify it matches at runtime
#if defined(INFECTIOUS_TARGET_CPU_IS_LITTLE_ENDIAN)
	assert(endian == CPUID::Endian_Status::Little);
#elif defined(INFECTIOUS_TARGET_CPU_IS_BIG_ENDIAN)
	assert(endian == CPUID::Endian_Status::Big);
#endif

	return endian;
}

// NOLINTEND(readability-magic-numbers,cppcoreguidelines-avoid-magic-numbers)

} // namespace infectious
