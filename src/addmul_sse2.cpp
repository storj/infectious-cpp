// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
//
// Hardware-optimized vector add-multiply for processors with SSE2.
//
// Based on code from the Botan project:
//
// Copyright (C) 2009,2010,2021 Jack Lloyd
//
// Botan is released under the Simplified BSD License (see LICENSE).

#include "infectious/fec.hpp"
#include "simd_32.hpp"

#if defined(INFECTIOUS_HAS_SSE2)

#include <immintrin.h>

namespace infectious {

// NOLINTBEGIN(portability-simd-intrinsics)

namespace {

inline auto rshift_1_u8(SIMD_4x32 v) -> SIMD_4x32 {
	return SIMD_4x32(_mm_add_epi8(v.raw(), v.raw()));
}

inline auto high_bit_set_u8(SIMD_4x32 v) -> SIMD_4x32 {
	return SIMD_4x32(_mm_cmpgt_epi8(_mm_setzero_si128(), v.raw()));
}

} // namespace

INFECTIOUS_FUNC_ISA("sse2")
auto FEC::addmul_sse2(uint8_t* z, const uint8_t* x, uint8_t y, size_t size) -> size_t {
	const SIMD_4x32 polynomial = SIMD_4x32::splat_u8(0x1D);

	const size_t orig_size = size;

	// unrolled out to cache line size
	while (size >= 64) {
		SIMD_4x32 x_1 = SIMD_4x32::load_le(x);
		SIMD_4x32 x_2 = SIMD_4x32::load_le(x + 16);
		SIMD_4x32 x_3 = SIMD_4x32::load_le(x + 32);
		SIMD_4x32 x_4 = SIMD_4x32::load_le(x + 48);

		SIMD_4x32 z_1 = SIMD_4x32::load_le(z);
		SIMD_4x32 z_2 = SIMD_4x32::load_le(z + 16);
		SIMD_4x32 z_3 = SIMD_4x32::load_le(z + 32);
		SIMD_4x32 z_4 = SIMD_4x32::load_le(z + 48);

		if (y & 0x01) {
			z_1 ^= x_1;
			z_2 ^= x_2;
			z_3 ^= x_3;
			z_4 ^= x_4;
		}

		for (size_t j = 1; j != 8; ++j) {
			/*
			* Each byte of each mask is either 0 or the polynomial 0x1D,
			* depending on if the high bit of x_i is set or not.
			*/

			const SIMD_4x32 mask_1(high_bit_set_u8(x_1));
			const SIMD_4x32 mask_2(high_bit_set_u8(x_2));
			const SIMD_4x32 mask_3(high_bit_set_u8(x_3));
			const SIMD_4x32 mask_4(high_bit_set_u8(x_4));

			// x <<= 1
			x_1 = rshift_1_u8(x_1);
			x_2 = rshift_1_u8(x_2);
			x_3 = rshift_1_u8(x_3);
			x_4 = rshift_1_u8(x_4);

			x_1 ^= mask_1 & polynomial;
			x_2 ^= mask_2 & polynomial;
			x_3 ^= mask_3 & polynomial;
			x_4 ^= mask_4 & polynomial;

			if ((y >> j) & 1) {
				z_1 ^= x_1;
				z_2 ^= x_2;
				z_3 ^= x_3;
				z_4 ^= x_4;
			}
		}

		z_1.store_le(z);
		z_2.store_le(z + 16);
		z_3.store_le(z + 32);
		z_4.store_le(z + 48);

		x += 64;
		z += 64;
		size -= 64;
	}

	return orig_size - size;
}

} // namespace infectious

// NOLINTEND(portability-simd-intrinsics)

#endif // defined(INFECTIOUS_HAS_SSE2)
