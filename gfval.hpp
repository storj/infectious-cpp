// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_GF_VAL_HPP
#define INFECTIOUS_GF_VAL_HPP

#include <stdexcept>

#include "tables.hpp"

namespace infectious {

//
// basic helpers around gf(2^8) values
//

class GFVal {
public:
	GFVal()
		: GFVal(0)
	{}

	GFVal(uint8_t n_)
		: n {n_}
	{}

	GFVal(const GFVal&) = default;
	GFVal(GFVal&&) = default;
	GFVal& operator=(const GFVal&) = default;
	GFVal& operator=(GFVal&&) = default;

	operator uint8_t() const {
		return n;
	}

	GFVal& operator^=(int x) {
		n ^= x;
		return *this;
	}

	GFVal pow(int val) const {
		GFVal out = 1;
		auto& mul_base = gf_mul_table[n];
		for (int i = 0; i < val; i++) {
			out = mul_base[out.n];
		}
		return out;
	}

	GFVal mul(GFVal b) const {
		return GFVal(gf_mul_table[n][b.n]);
	}

	GFVal div(GFVal b) const {
		if (b == 0) {
			throw std::domain_error("divide by zero");
		}
		if (n == 0) {
			return 0;
		}
		return GFVal(gf_exp[gf_log[n]-gf_log[b.n]]);
	}

	GFVal add(GFVal b) const noexcept {
		return GFVal(n ^ b.n);
	}

	bool is_zero() const noexcept {
		return n == 0;
	}

	GFVal inv() const {
		if (n == 0) {
			throw std::domain_error("invert zero");
		}
		return GFVal(gf_exp[255-gf_log[n]]);
	}

protected:
	uint8_t n;
};

} // namespace infectious

#endif // INFECTIOUS_GF_VAL_HPP
