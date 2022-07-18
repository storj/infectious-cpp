// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_ADDMUL_HPP
#define INFECTIOUS_ADDMUL_HPP

#include "slice.hpp"
#include "gfval.hpp"

namespace infectious {

void addmul(
	uint8_t* z_begin, uint8_t* z_end,
	const uint8_t* x, uint8_t y
);

} // namespace infectious

#endif // INFECTIOUS_ADDMUL_HPP
