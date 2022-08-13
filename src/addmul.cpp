// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.
/*
 * (C) 2011 Billy Brumley (billy.brumley@aalto.fi)
 *
 * Distributed under the terms given in license.txt (Simplified BSD)
 */

#include <x86intrin.h>

#include "infectious/fec.hpp"
#include "tables.hpp"

namespace infectious {

// we go without bounds checking on accesses to the gf_mul_table
// and its friends.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

void FEC::addmul(
	uint8_t* z_begin, const uint8_t* z_end,
	const uint8_t* x, uint8_t y
) {
	auto z_size = static_cast<int>(z_end - z_begin);
	const auto& gf_mul_y = gf_mul_table[y];
	for (int i = 0; i < z_size; i++) {
		z_begin[i] ^= gf_mul_y[x[i]];
	}
}

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

} // namespace infectious
