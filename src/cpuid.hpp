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

#ifndef INFECTIOUS_CPUID_HPP
#define INFECTIOUS_CPUID_HPP

#include <cinttypes>

#include "infectious/build_env.h"

namespace infectious {

/**
* A class handling runtime CPU feature detection. It is limited to
* just the features necessary to implement CPU specific code in
* Infectious-CPP, rather than being a general purpose utility.
*
* This class supports:
*
*  - x86 features using CPUID. x86 is also the only processor with
*    accurate cache line detection currently.
*
*  - PowerPC AltiVec detection on Linux, NetBSD, OpenBSD, and macOS
*
*  - ARM NEON and crypto extensions detection. On Linux and Android
*    systems which support getauxval, that is used to access CPU
*    feature information. Otherwise a relatively portable but
*    thread-unsafe mechanism involving executing probe functions which
*    catching SIGILL signal is used.
*/
class CPUID final {
public:
	/**
	* Probe the CPU and see what extensions are supported
	*/
	static void initialize() {
		state() = CPUID_Data();
	}

	[[nodiscard]] static auto is_little_endian() -> bool {
#if defined(INFECTIOUS_TARGET_CPU_IS_LITTLE_ENDIAN)
		return true;
#elif defined(INFECTIOUS_TARGET_CPU_IS_BIG_ENDIAN)
		return false;
#else
		return state().endian_status() == Endian_Status::Little;
#endif
	}

	[[nodiscard]] static auto is_big_endian() -> bool {
#if defined(INFECTIOUS_TARGET_CPU_IS_BIG_ENDIAN)
		return true;
#elif defined(INFECTIOUS_TARGET_CPU_IS_LITTLE_ENDIAN)
		return false;
#else
		return state().endian_status() == Endian_Status::Big;
#endif
	}

	enum CPUID_bits : uint64_t {
#if defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)
		// These values have no relation to cpuid bitfields
		CPUID_SSE2_BIT        = (1ULL << 0),
		CPUID_SSSE3_BIT       = (1ULL << 1),
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY)
		CPUID_ALTIVEC_BIT     = (1ULL << 0),
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY)
		CPUID_ARM_NEON_BIT    = (1ULL << 0),
#endif

		CPUID_INITIALIZED_BIT = (1ULL << 63)
	};

#if defined(INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY)
	/**
	* Check if the processor supports AltiVec/VMX
	*/
	static auto has_altivec() -> bool { return has_cpuid_bit(CPUID_ALTIVEC_BIT); }
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY)
	/**
	* Check if the processor supports NEON SIMD
	*/
	static auto has_neon() -> bool { return has_cpuid_bit(CPUID_ARM_NEON_BIT); }
#endif

#if defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)
	/**
	* Check if the processor supports SSSE3
	*/
	static auto has_ssse3() -> bool { return has_cpuid_bit(CPUID_SSSE3_BIT); }

	/**
	* Check if the processor supports SSE2
	*/
	static auto has_sse2() -> bool { return has_cpuid_bit(CPUID_SSE2_BIT); }
#endif

	/**
	* Check if the processor supports byte-level vector permutes
	* (SSSE3, NEON, Altivec)
	*/
	[[nodiscard]] static auto has_vperm() -> bool {
#if defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)
		return has_ssse3();
#elif defined(INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY)
		return has_neon();
#else
		return false;
#endif
	}

private:
	[[nodiscard]] static auto has_cpuid_bit(CPUID_bits elem) -> bool {
		const auto elem64 = static_cast<uint64_t>(elem);
		return state().has_bit(elem64);
	}

	enum class Endian_Status : uint32_t {
		Unknown = 0x00000000,
		Big     = 0x01234567,
		Little  = 0x67452301,
	};

	struct CPUID_Data {
	public:
		CPUID_Data()
#if defined(INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY) || \
	defined(INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY) || \
	defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)
			: m_processor_features {detect_cpu_features() | CPUID::CPUID_INITIALIZED_BIT}
#else
			: m_processor_features {CPUID::CPUID_INITIALIZED_BIT}
#endif
			, m_endian_status {runtime_check_endian()}
		{}

		CPUID_Data(const CPUID_Data& other) = default;
		auto operator=(const CPUID_Data& other) -> CPUID_Data& = default;
		CPUID_Data(CPUID_Data&& other) = default;
		auto operator=(CPUID_Data&& other) -> CPUID_Data& = default;
		~CPUID_Data() = default;

		[[nodiscard]] auto has_bit(uint64_t bit) const -> bool {
			return (m_processor_features & bit) == bit;
		}

	private:
		static auto runtime_check_endian() -> Endian_Status;

#if defined(INFECTIOUS_TARGET_CPU_IS_PPC_FAMILY) || \
	defined(INFECTIOUS_TARGET_CPU_IS_ARM_FAMILY) || \
	defined(INFECTIOUS_TARGET_CPU_IS_X86_FAMILY)
		static auto detect_cpu_features() -> uint64_t;
#endif

		uint64_t m_processor_features {0};
		Endian_Status m_endian_status {0};
	};

	static auto state() -> CPUID_Data& {
		static CPUID::CPUID_Data g_cpuid;
		return g_cpuid;
	}
};

}

#endif // INFECTIOUS_CPUID_HPP
