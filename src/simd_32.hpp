// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Lightweight wrappers for SIMD operations
//
// Based on code from the Botan project:
//
// (C) 2009,2011,2016,2017,2019 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#ifndef INFECTIOUS_SIMD_32_HPP
#define INFECTIOUS_SIMD_32_HPP

#include "infectious/build_env.h"

#if defined(INFECTIOUS_TARGET_SUPPORTS_SSE2)
# include <emmintrin.h>
# define INFECTIOUS_SIMD_USE_SSE2

#elif defined(INFECTIOUS_TARGET_SUPPORTS_NEON)
# include <arm_neon.h>
# include "cpuid.hpp"
# define INFECTIOUS_SIMD_USE_NEON

#else
# error "No SIMD instruction set enabled"
#endif

// These need to be macros so they can be used in function attributes.
// NOLINTBEGIN(cppcoreguidelines-macro-usage)

#if defined(INFECTIOUS_SIMD_USE_SSE2)
# define INFECTIOUS_SIMD_ISA "sse2"
# define INFECTIOUS_VPERM_ISA "ssse3"
#elif defined(INFECTIOUS_SIMD_USE_NEON)
# if defined(INFECTIOUS_TARGET_ARCH_IS_ARM64)
#  define INFECTIOUS_SIMD_ISA "+simd"
# else
#  define INFECTIOUS_SIMD_ISA "fpu=neon"
# endif
# define INFECTIOUS_VPERM_ISA INFECTIOUS_SIMD_ISA
#endif

// NOLINTEND(cppcoreguidelines-macro-usage)

// We do know what these narrowing conversions will do, because we know the
// target architecture(s) in this case.
// NOLINTBEGIN(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)

namespace infectious {

#if defined(INFECTIOUS_SIMD_USE_SSE2)
using native_simd_type = __m128i;
#elif defined(INFECTIOUS_SIMD_USE_NEON)
using native_simd_type = uint32x4_t;
#endif

/**
* 4x32 bit SIMD register
*
* This class is not a general purpose SIMD type, and only offers
* instructions needed for evaluation of specific primitives.
* For example it does not currently have equality operators of any
* kind.
*
* Implemented for SSE2 and NEON.
*/
class SIMD_4x32 final {
public:

	SIMD_4x32(const SIMD_4x32& other) = default;
	SIMD_4x32(SIMD_4x32&& other) = default;
	auto operator=(const SIMD_4x32& other) -> SIMD_4x32& = default;
	auto operator=(SIMD_4x32&& other) -> SIMD_4x32& = default;
	~SIMD_4x32() = default;

	/**
	* Zero initialize SIMD register with 4 32-bit elements
	*/
	SIMD_4x32()
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		: m_simd (_mm_setzero_si128())
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		: m_simd (vdupq_n_u32(0))
#endif
	{}

	/**
	* Load SIMD register with 4 32-bit elements
	*/
	explicit SIMD_4x32(const uint32_t B[4])
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		: m_simd (_mm_loadu_si128(reinterpret_cast<const __m128i*>(B)))
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		: m_simd (vld1q_u32(&B[0]))
#endif
	{}

	// This is hard to write using member initializers in the USE_NEON
	// pathway.
	// NOLINTBEGIN(cppcoreguidelines-prefer-member-initializer)

	/**
	* Load SIMD register with 4 32-bit elements
	*/
	SIMD_4x32(uint32_t B0, uint32_t B1, uint32_t B2, uint32_t B3) {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		m_simd = _mm_set_epi32(B3, B2, B1, B0);
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		// Better way to do this?
		const uint32_t B[4] = { B0, B1, B2, B3 };
		m_simd = vld1q_u32(B);
#endif
	}

	// NOLINTEND(cppcoreguidelines-prefer-member-initializer)

	/**
	* Load SIMD register with one 32-bit element repeated
	*/
	static auto splat(uint32_t B) -> SIMD_4x32 {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		return SIMD_4x32(_mm_set1_epi32(B));
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		return SIMD_4x32(vdupq_n_u32(B));
#else
		return SIMD_4x32(B, B, B, B);
#endif
	}

	/**
	* Load SIMD register with one 8-bit element repeated
	*/
	static auto splat_u8(uint8_t B) -> SIMD_4x32 {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		return SIMD_4x32(_mm_set1_epi8(B));
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		return SIMD_4x32(vreinterpretq_u32_u8(vdupq_n_u8(B)));
#else
		const uint32_t B4 = make_uint32(B, B, B, B);
		return SIMD_4x32(B4, B4, B4, B4);
#endif
	}

	/**
	* Load a SIMD register with little-endian convention
	*/
	static auto load_le(const void* in) -> SIMD_4x32 {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		return SIMD_4x32(_mm_loadu_si128(reinterpret_cast<const __m128i*>(in)));
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		SIMD_4x32 l(vld1q_u32(static_cast<const uint32_t*>(in)));
		return CPUID::is_big_endian() ? l.bswap() : l;
#endif
	}

	void store_le(uint32_t out[4]) const {
		this->store_le(reinterpret_cast<uint8_t*>(&out[0]));
	}

	void store_le(uint64_t out[2]) const {
		this->store_le(reinterpret_cast<uint8_t*>(&out[0]));
	}

	/**
	* Load a SIMD register with little-endian convention
	*/
	void store_le(uint8_t* out) const {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		_mm_storeu_si128(reinterpret_cast<__m128i*>(out), raw());
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		if (CPUID::is_little_endian()) {
			vst1q_u8(out, vreinterpretq_u8_u32(m_simd));
		} else {
			vst1q_u8(out, vreinterpretq_u8_u32(bswap().m_simd));
		}
#endif
	}

	/**
	* Add elements of a SIMD vector
	*/
	auto operator+(const SIMD_4x32& other) const -> SIMD_4x32 {
		SIMD_4x32 retval(*this);
		retval += other;
		return retval;
	}

	/**
	* Subtract elements of a SIMD vector
	*/
	auto operator-(const SIMD_4x32& other) const -> SIMD_4x32 {
		SIMD_4x32 retval(*this);
		retval -= other;
		return retval;
	}

	/**
	* XOR elements of a SIMD vector
	*/
	auto operator^(const SIMD_4x32& other) const -> SIMD_4x32 {
		SIMD_4x32 retval(*this);
		retval ^= other;
		return retval;
	}

	/**
	* Binary OR elements of a SIMD vector
	*/
	auto operator|(const SIMD_4x32& other) const -> SIMD_4x32 {
		SIMD_4x32 retval(*this);
		retval |= other;
		return retval;
	}

	/**
	* Binary AND elements of a SIMD vector
	*/
	auto operator&(const SIMD_4x32& other) const -> SIMD_4x32 {
		SIMD_4x32 retval(*this);
		retval &= other;
		return retval;
	}

	void operator+=(const SIMD_4x32& other) {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		m_simd = _mm_add_epi32(m_simd, other.m_simd);
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		m_simd = vaddq_u32(m_simd, other.m_simd);
#endif
	}

	void operator-=(const SIMD_4x32& other) {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		m_simd = _mm_sub_epi32(m_simd, other.m_simd);
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		m_simd = vsubq_u32(m_simd, other.m_simd);
#endif
	}

	void operator^=(const SIMD_4x32& other) {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		m_simd = _mm_xor_si128(m_simd, other.m_simd);
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		m_simd = veorq_u32(m_simd, other.m_simd);
#endif
	}

	void operator^=(uint32_t other) {
		*this ^= SIMD_4x32::splat(other);
	}

	void operator|=(const SIMD_4x32& other) {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		m_simd = _mm_or_si128(m_simd, other.m_simd);
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		m_simd = vorrq_u32(m_simd, other.m_simd);
#endif
	}

	void operator&=(const SIMD_4x32& other) {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		m_simd = _mm_and_si128(m_simd, other.m_simd);
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		m_simd = vandq_u32(m_simd, other.m_simd);
#endif
	}

	template <int SHIFT>
	[[nodiscard]] auto shr() const -> SIMD_4x32 {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		return SIMD_4x32(_mm_srli_epi32(m_simd, SHIFT));
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		return SIMD_4x32(vshrq_n_u32(m_simd, SHIFT));
#endif
	}

	/**
	* Return copy *this with each word byte swapped
	*/
	[[nodiscard]] auto bswap() const -> SIMD_4x32 {
#if defined(INFECTIOUS_SIMD_USE_SSE2)
		__m128i T = m_simd;
		T = _mm_shufflehi_epi16(T, _MM_SHUFFLE(2, 3, 0, 1));
		T = _mm_shufflelo_epi16(T, _MM_SHUFFLE(2, 3, 0, 1));
		return SIMD_4x32(_mm_or_si128(_mm_srli_epi16(T, 8), _mm_slli_epi16(T, 8)));
#elif defined(INFECTIOUS_SIMD_USE_NEON)
		return SIMD_4x32(vreinterpretq_u32_u8(vrev32q_u8(vreinterpretq_u8_u32(m_simd))));
#endif
	}

	[[nodiscard]] auto raw() const -> native_simd_type {
		return m_simd;
	}

	explicit SIMD_4x32(native_simd_type x) : m_simd(x) {}
private:
	native_simd_type m_simd;
};

} // namespace infectious

// NOLINTEND(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)

#endif // INFECTIOUS_SIMD_32_HPP
