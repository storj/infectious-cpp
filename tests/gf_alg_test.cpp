// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include "gtest/gtest.h"

#include "infectious/fec.hpp"
#include "gf_alg.hpp"

namespace infectious::test {

// for some reason, clang-tidy doesn't much care for gtest's macros.
// NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

TEST(GaloisFieldMath, PolynomialDivision) {
	// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)
	GFPoly q({
		0x5e, 0x60, 0x8c, 0x3d, 0xc6, 0x8e, 0x7e, 0xa5, 0x2c, 0xa4, 0x04, 0x8a,
		0x2b, 0xc2, 0x36, 0x0f, 0xfc, 0x3f, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	});
	GFPoly e({
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	});
	// NOLINTEND(cppcoreguidelines-avoid-magic-numbers,readability-magic-numbers)

	// ignore result; just checking if it throws
	(void) q.div(e);
}

// NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

} // namespace infectious::test
