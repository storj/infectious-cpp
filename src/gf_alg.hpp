// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_GF_ALG_HPP
#define INFECTIOUS_GF_ALG_HPP

#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "infectious/fec.hpp"
#include "tables.hpp"

namespace infectious {

namespace internal {

template <typename T>
[[nodiscard]] auto hex_string(const T& t) -> std::string {
	std::stringstream ss;
	ss << std::hex;
	for (auto v : t) {
		ss << std::setfill('0') << std::setw(2) << int(v);
	}
	return ss.str();
}

[[nodiscard]] auto inline hex_string(const uint8_t* t, long len) -> std::string {
	return hex_string(std::basic_string_view<uint8_t>(t, len));
}

} // namespace internal

//
// basic helpers around gf(2^8) values
//

// we go without bounds checking on accesses to the gf_mul_table
// and its friends.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

[[nodiscard]] inline auto gf_pow(uint8_t n, int val) -> uint8_t {
	uint8_t out = 1;
	const auto& mul_base = gf_mul_table[n];
	for (int i = 0; i < val; i++) {
		out = mul_base[out];
	}
	return out;
}

[[nodiscard]] inline auto gf_mul(uint8_t a, uint8_t b) -> uint8_t {
	return gf_mul_table[a][b];
}

[[nodiscard]] inline auto gf_div(uint8_t a, uint8_t b) -> uint8_t {
	if (b == 0) {
		throw std::domain_error("divide by zero");
	}
	if (a == 0) {
		return 0;
	}
	return gf_exp[gf_log[a]-gf_log[b]];
}

[[nodiscard]] inline auto gf_add(uint8_t a, uint8_t b) -> uint8_t {
	return a ^ b;
}

// because apparently 255 is a 'magic number'
const static int top_of_range = (1<<8) - 1;

[[nodiscard]] inline auto gf_inv(uint8_t n) -> uint8_t {
	if (n == 0) {
		throw std::domain_error("invert zero");
	}
	return gf_exp[top_of_range-gf_log[n]];
}

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

//
// basic helpers about sequences of gf(2^8) values
//

template <typename TA, typename TB>
[[nodiscard]] auto gf_dot(const TA& a, const TB& b) -> uint8_t {
	auto a_size = a.size();
	uint8_t out = 0;
	for (decltype(a_size) i = 0; i < a_size; ++i) {
		out = gf_add(out, gf_mul(a[i], b[i]));
	}
	return out;
}

//
// basic helpers for dealing with polynomials with coefficients in gf(2^8)
//

class GFPoly {
public:
	GFPoly()
		: start_at {0} {}

	explicit GFPoly(std::vector<uint8_t> values_)
		: values {std::move(values_)}
		, start_at {0} {}

	explicit GFPoly(int size)
		: values {std::vector<uint8_t>(size, 0)}
		, start_at {0} {}

	[[nodiscard]] auto size() const -> int {
		return static_cast<int>(values.size()) - start_at;
	}

	[[nodiscard]] auto deg() const -> int {
		return static_cast<int>(size()) - 1;
	}

	[[nodiscard]] auto scale(uint8_t factor) const -> GFPoly {
		std::vector<uint8_t> out(size(), 0);
		for (int i = 0; i < static_cast<int>(out.size()); ++i) {
			out[i] = gf_mul((*this)[i], factor);
		}
		return GFPoly(std::move(out));
	}

	[[nodiscard]] auto index(int power) const -> uint8_t {
		if (power < 0) {
			return 0;
		}
		int which = deg() - power;
		if (which < 0) {
			return 0;
		}
		return (*this)[which];
	}

	void set(int pow, uint8_t coef) {
		int which = deg() - pow;
		if (which < 0) {
			std::vector<uint8_t> tmp = std::move(values);
			values = std::vector<uint8_t>(-which, 0);
			values.insert(values.end(), tmp.begin() + start_at, tmp.end());
			which = deg() - pow;
			start_at = 0;
		}
		values[start_at + which] = coef;
	}

	[[nodiscard]] auto add(const GFPoly& b) const -> GFPoly {
		auto len = size();
		if (b.size() > len) {
			len = b.size();
		}
		GFPoly out(len);
		for (int i = 0; i < out.size(); ++i) {
			auto ai = index(i);
			auto bi = b.index(i);
			out.set(i, gf_add(ai, bi));
		}
		return out;
	}

	[[nodiscard]] auto is_zero() const -> bool {
		return std::all_of(values.begin() + start_at, values.end(), [](uint8_t v) -> bool { return v == 0; });
	}

	void remove_leading_zeroes() {
		while (start_at < static_cast<int>(values.size()) && values[start_at] == 0) {
			++start_at;
		}
	}

	void push_back(uint8_t v) {
		values.push_back(v);
	}

	// Calculates *this divided by b; returns the quotient and remainder.
	// Throws std::domain_error on divide by zero.
	[[nodiscard]] auto div(GFPoly b) -> std::pair<GFPoly, GFPoly> {
		// sanitize the divisor by removing leading zeros.
		b.remove_leading_zeroes();
		if (b.size() == 0) {
			throw std::domain_error("polynomial divide by zero");
		}

		// sanitize the base poly as well
		GFPoly p = *this;
		p.remove_leading_zeroes();
		if (p.size() == 0) {
			return std::make_pair(GFPoly(1), GFPoly(1));
		}

		GFPoly q;
		while (b.deg() <= p.deg()) {
			auto leading_p = p.index(p.deg());
			auto leading_b = b.index(b.deg());

			auto coef = gf_div(leading_p, leading_b);
			q.push_back(coef);

			auto padded = b.scale(coef);
			std::vector<uint8_t> zeros_to_append(p.deg() - padded.deg(), 0);
			std::copy(zeros_to_append.begin(), zeros_to_append.end(), std::back_inserter(padded.values));

			p = p.add(padded);
			if (p[0] != 0) {
				throw std::domain_error("algebraic error in polynomial division");
			}
			p.shift(1);
		}

		while (p.size() > 1 && p[0] == 0) {
			p.shift(1);
		}

		return std::make_pair(q, p);
	}

	auto operator[](int index) -> uint8_t& {
		if (static_cast<int>(values.size()) - start_at <= index) {
			throw std::out_of_range("index out of bounds");
		}
		return values[index + start_at];
	}

	auto operator[](int index) const -> const uint8_t& {
		if (static_cast<int>(values.size()) - start_at <= index) {
			throw std::out_of_range("index out of bounds");
		}
		return values[index + start_at];
	}

	void shift(int elements) {
		if (static_cast<int>(values.size()) - start_at < elements) {
			throw std::out_of_range("can't shift this polynomial to requested extent");
		}
		start_at += elements;
	}

	[[nodiscard]] auto eval(uint8_t x) const -> uint8_t {
		uint8_t out = 0;
		for (int i = 0; i <= deg(); ++i) {
			auto x_i = gf_pow(x, i);
			auto p_i = index(i);
			out = gf_add(out, gf_mul(p_i, x_i));
		}
		return out;
	}

private:
	std::vector<uint8_t> values;

	// This class is implemented using an index into the array, because
	// "remove elements from the beginning of the array" is something that
	// is needed quite frequently in the course of our berlekamp-welch error
	// correction. All lookups into `values` should add `start_at` to the
	// desired index.
	int start_at;
};

//
// basic helpers for matrices in gf(2^8)
//

class FEC::GFMat {
public:
	GFMat(long i, long j)
		: d(i*j)
		, r {i}
		, c {j}
	{}

	[[nodiscard]] auto to_string() const -> std::string {
		if (r == 0) {
			return "---";
		}

		std::stringstream ss;
		for (long i = 0; i < r-1; i++) {
			ss << internal::hex_string(index_row(i), c) << '\n';
		}
		ss << internal::hex_string(index_row(r - 1), c);
		return ss.str();
	}

	[[nodiscard]] auto index(long i, long j) const -> long {
		return c * i + j;
	}

	[[nodiscard]] auto get(long i, long j) const -> uint8_t {
		return d[index(i, j)];
	}

	void set(long i, long j, uint8_t val) {
		d[index(i, j)] = val;
	}

	[[nodiscard]] auto index_row(long i) -> uint8_t* {
		return &d[index(i, 0)];
	}

	[[nodiscard]] auto index_row(long i) const -> const uint8_t* {
		return &d[index(i, 0)];
	}

	void swap_row(long i, long j) {
		std::vector<uint8_t> tmp(c, 0);
		auto* ri = index_row(i);
		auto* rj = index_row(j);
		std::copy(ri, ri + c, tmp.begin());
		std::copy(rj, rj + c, ri);
		std::copy(tmp.begin(), tmp.end(), rj);
	}

	void scale_row(long r, uint8_t val) {
		auto* ri = index_row(r);
		for (long i = 0; i < c; ++i) {
			ri[i] = gf_mul(ri[i], val);
		}
	}

	// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

	void addmul_row(long i, long j, uint8_t val) {
		auto* ri = index_row(i);
		auto* rj = index_row(j);
		FEC::addmul(rj, rj + c, ri, val);
	}

	// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

	// in place invert. the output is put into a and *this is turned into the
	// identity matrix. a is expected to be the identity matrix.
	void invert_with(GFMat& a) {
		for (long i = 0; i < r; i++) {
			long p_row = i;
			auto p_val = get(i, i);
			for (long j = i + 1; j < r && p_val == 0; j++) {
				p_row = j;
				p_val = get(j, i);
			}
			if (p_val == 0) {
				continue;
			}

			if (p_row != i) {
				swap_row(i, p_row);
				a.swap_row(i, p_row);
			}

			auto inv = gf_inv(p_val);
			scale_row(i, inv);
			a.scale_row(i, inv);

			for (long j = i + 1; j < r; j++) {
				auto leading = get(j, i);
				addmul_row(i, j, leading);
				a.addmul_row(i, j, leading);
			}
		}

		for (long i = r - 1; i > 0; i--) {
			for (long j = i - 1; j >= 0; j--) {
				auto trailing = get(j, i);
				addmul_row(i, j, trailing);
				a.addmul_row(i, j, trailing);
			}
		}
	}

	// in place standardize.
	void standardize() {
		for (int i = 0; i < r; i++) {
			long p_row = i;
			auto p_val = get(i, i);
			for (long j = i + 1; j < r && p_val == 0; j++) {
				p_row = j;
				p_val = get(j, i);
			}
			if (p_val == 0) {
				continue;
			}

			if (p_row != i) {
				swap_row(i, p_row);
			}

			auto inv = gf_inv(p_val);
			scale_row(i, inv);

			for (long j = i + 1; j < r; j++) {
				auto leading = get(j, i);
				addmul_row(i, j, leading);
			}
		}

		for (long i = r - 1; i > 0; i--) {
			for (long j = i - 1; j >= 0; j--) {
				auto trailing = get(j, i);
				addmul_row(i, j, trailing);
			}
		}
	}

	// parity returns the new matrix because it changes dimensions and stuff. it
	// can be done in place, but is easier to implement with a copy.
	[[nodiscard]] auto parity() const -> GFMat {
		// we assume *this is in standard form already
		// it is of form [I_r | P]
		// our output will be [-P_transpose | I_(c - r)]
		// but our field is of characteristic 2 so we do not need the negative.

		// In terms of *this:
		// I_r has r rows and r columns.
		// P has r rows and c-r columns.
		// P_transpose has c-r rows, and r columns.
		// I_(c-r) has c-r rows and c-r columns.
		// so: out.r == c-r, out.c == r + c - r == c

		GFMat out(c-r, c);

		// step 1. fill in the identity. it starts at column offset r.
		for (long i = 0; i < c-r; i++) {
			out.set(i, i+r, 1);
		}

		// step 2: fill in the transposed P matrix. i and j are in terms of out.
		for (long i = 0; i < c-r; i++) {
			for (long j = 0; j < r; j++) {
				out.set(i, j, get(j, i+r));
			}
		}

		return out;
	}

	[[nodiscard]] auto get_r() const -> long {
		return r;
	}

	[[nodiscard]] auto get_c() const -> long {
		return c;
	}

private:
	std::vector<uint8_t> d;
	long r;
	long c;
};

} // namespace infectious

#endif // ifndef INFECTIOUS_GF_ALG_HPP
