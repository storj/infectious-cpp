// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_MATH_HPP
#define INFECTIOUS_MATH_HPP

#include <cinttypes>

#include "slice.hpp"

namespace infectious {

void invertMatrix(Slice<uint8_t> matrix, int k);
void createInvertedVdm(Slice<uint8_t> vdm, int k);

} // namespace infectious

#endif // INFECTIOUS_MATH_HPP
