// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include "addmul.hpp"
#include "slice.hpp"
#include "tables.hpp"

namespace infectious {

void addmul(
	uint8_t* z_begin, uint8_t* z_end,
	const uint8_t* x, uint8_t y
) {
	int z_size = z_end - z_begin;
	auto& gf_mul_y = gf_mul_table[y];
	for (int i = 0; i < z_size; i++) {
		z_begin[i] ^= GFVal(gf_mul_y[x[i]]);
	}
}

} // namespace infectious
