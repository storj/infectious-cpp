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
	~GFVal() = default;
	auto operator=(const GFVal&) -> GFVal& = default;
	auto operator=(GFVal&&) -> GFVal& = default;

	operator uint8_t() const noexcept {
		return n;
	}

	auto operator^=(int x) noexcept -> GFVal& {
		n ^= x;
		return *this;
	}

	// we go without bounds checking on accesses to the gf_mul_table
	// and its friends.
	// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

	[[nodiscard]] auto pow(int val) const -> GFVal {
		GFVal out = 1;
		const auto& mul_base = gf_mul_table[n];
		for (int i = 0; i < val; i++) {
			out = mul_base[out.n];
		}
		return out;
	}

	[[nodiscard]] auto mul(GFVal b) const -> GFVal {
		return {gf_mul_table[n][b.n]};
	}

	[[nodiscard]] auto div(GFVal b) const -> GFVal {
		if (b == 0) {
			throw std::domain_error("divide by zero");
		}
		if (n == 0) {
			return 0;
		}
		return {gf_exp[gf_log[n]-gf_log[b.n]]};
	}

	// clang-tidy means well, but it's ok to pass b by value, it's one byte
	// NOLINTNEXTLINE(performance-unnecessary-value-param)
	[[nodiscard]] auto add(GFVal b) const noexcept -> GFVal {
		return {static_cast<uint8_t>(n ^ b.n)};
	}

	[[nodiscard]] auto is_zero() const noexcept -> bool {
		return n == 0;
	}

	const static int top_of_range = (1<<8) - 1;

	[[nodiscard]] auto inv() const -> GFVal {
		if (n == 0) {
			throw std::domain_error("invert zero");
		}
		return {gf_exp[top_of_range-gf_log[n]]};
	}

	// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

private:
	uint8_t n;
};

} // namespace infectious

#endif // INFECTIOUS_GF_VAL_HPP
