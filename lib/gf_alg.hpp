// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_GF_ALG_HPP
#define INFECTIOUS_GF_ALG_HPP

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

namespace infectious {

template <typename T>
[[nodiscard]] auto hex_string(const T& t) -> std::string {
	std::stringstream ss;
	ss << std::hex;
	for (auto v : t) {
		ss << std::setfill('0') << std::setw(2) << int(v);
	}
	return ss.str();
}

//
// basic helpers about a slice of gf(2^8) values
//

class GFVals : public SliceSubclass<GFVal, GFVals> {
public:
	using SliceSubclass<GFVal, GFVals>::SliceSubclass;

	[[nodiscard]] auto dot(const GFVals& b) const -> GFVal {
		GFVal out = 0;
		for (int i = 0; i < size(); i++) {
			out = out.add((*this)[i].mul(b[i]));
		}
		return out;
	}
};

//
// basic helpers for dealing with polynomials with coefficients in gf(2^8)
//

class GFPoly : public SliceSubclass<GFVal, GFPoly> {
public:
	using SliceSubclass<GFVal, GFPoly>::SliceSubclass;

	explicit GFPoly(const GFVals& gfv)
		: GFPoly(static_cast<const GFPoly&>(static_cast<const Slice&>(gfv)))
	{}

	[[nodiscard]] static auto poly_zero(int size) -> GFPoly {
		return {size, 0};
	}

	[[nodiscard]] auto is_zero() const -> bool {
		return std::all_of(begin(), end(), [](GFVal g) -> bool {
			return g.is_zero();
		});
	}

	[[nodiscard]] auto deg() const -> int {
		return size() - 1;
	}

	[[nodiscard]] auto index(int power) const -> GFVal {
		if (power < 0) {
			return 0;
		}
		int which = deg() - power;
		if (which < 0) {
			return 0;
		}
		return (*this)[which];
	}

	[[nodiscard]] auto scale(GFVal factor) const -> GFPoly {
		GFPoly out(size(), 0);
		for (int i = 0; i < size(); i++) {
			out[i] = (*this)[i].mul(factor);
		}
		return out;
	}

	void set(int pow, GFVal coef) {
		int which = deg() - pow;
		if (which < 0) {
			GFPoly tmp = std::move(*this);
			*this = GFPoly(-which, 0);
			extend(tmp);
			which = deg() - pow;
		}
		(*this)[which] = coef;
	}

	[[nodiscard]] auto add(const GFPoly& b) const -> GFPoly {
		auto len = size();
		if (b.size() > len) {
			len = b.size();
		}
		GFPoly out(len, 0);
		for (int i = 0; i < out.size(); i++) {
			auto pi = index(i);
			auto bi = b.index(i);
			out.set(i, pi.add(bi));
		}
		return out;
	}

	// Accepts b, *this is p; returns q, r.
	// Throws std::domain_error on divide by zero.
	[[nodiscard]] auto div(GFPoly b) const -> std::pair<GFPoly, GFPoly> {
		// sanitize the divisor by removing leading zeros.
		while (b.size() > 0 && b[0].is_zero()) {
			b = b.slice(1);
		}
		if (b.size() == 0) {
			throw std::domain_error("divide by zero");
		}

		// sanitize the base poly as well
		auto p = slice(0);
		while (p.size() > 0 && p[0].is_zero()) {
			p = p.slice(1);
		}
		if (p.size() == 0) {
			return std::make_pair(poly_zero(1), poly_zero(1));
		}

		const bool debug = false;
		int indent = 2*b.size() + 1;

		if constexpr (debug) {
			std::cerr << hex_string(b) << ' ' << hex_string(p) << '\n';
		} else {
			(void) indent; // indent is used.
		}

		GFPoly q;
		while (b.deg() <= p.deg()) {
			auto leading_p = p.index(p.deg());
			auto leading_b = b.index(b.deg());

			if constexpr (debug) {
				std::cerr
					<< std::setw(2) << std::setfill('0') << std::hex
					<< "leading_p: "
					<< static_cast<int>(leading_p)
					<< " leading_b: "
					<< static_cast<int>(leading_b)
					<< "\n";
			}

			auto coef = leading_p.div(leading_b);
			if constexpr (debug) {
				std::cerr << std::setw(2) << std::setfill('0') << std::hex << "coef: " << static_cast<int>(coef) << '\n';
			}

			q.push_back(coef);

			auto padded = b.scale(coef);
			auto append_zero = poly_zero(p.deg() - padded.deg());
			std::copy(append_zero.begin(), append_zero.end(), std::back_inserter(padded));

			if constexpr (debug) {
				std::cerr << std::setw(indent) << "" << std::setw(0) << hex_string(padded) << '\n';
				indent += 2;
			}

			p = p.add(padded);
			if (!p[0].is_zero()) {
				throw std::domain_error(std::string("alg error: ") + hex_string(p));
			}
			p = p.slice(1);
		}

		while (p.size() > 1 && p[0].is_zero()) {
			p = p.slice(1);
		}

		return std::make_pair(q, p);
	}

	[[nodiscard]] auto eval(GFVal x) const -> GFVal {
		GFVal out {0};
		for (int i = 0; i <= deg(); i++) {
			auto x_i = x.pow(i);
			auto p_i = index(i);
			out = out.add(p_i.mul(x_i));
		}
		return out;
	}
};

//
// basic helpers for matrices in gf(2^8)
//

class GFMat {
public:
	GFMat(int i, int j)
		: d(i*j)
		, r {i}
		, c {j}
	{}

	[[nodiscard]] auto to_string() const -> std::string {
		if (r == 0) {
			return "";
		}

		std::stringstream ss;
		for (int i = 0; i < r-1; i++) {
			ss << hex_string(index_row(i)) << '\n';
		}
		ss << hex_string(index_row(r - 1));
		return ss.str();
	}

	[[nodiscard]] auto index(int i, int j) const -> int {
		return c * i + j;
	}

	[[nodiscard]] auto get(int i, int j) const -> GFVal {
		return d[index(i, j)];
	}

	void set(int i, int j, GFVal val) {
		d[index(i, j)] = val;
	}

	[[nodiscard]] auto index_row(int i) const -> GFVals {
		return d.slice(index(i, 0), index(i+1, 0));
	}

	void swap_row(int i, int j) const {
		GFVals tmp(r, 0);
		auto ri = index_row(i);
		auto rj = index_row(j);
		std::copy(ri.begin(), ri.end(), tmp.begin());
		std::copy(rj.begin(), rj.end(), ri.begin());
		std::copy(tmp.begin(), tmp.end(), rj.begin());
	}

	void scale_row(int i, GFVal val) const {
		auto ri = index_row(i);
		for (auto& i : ri) {
			i = i.mul(val);
		}
	}

	// NOLINTBEGIN(cppcoreguidelines-pro-type-reinterpret-cast)

	void addmul_row(int i, int j, GFVal val) const {
		auto ri = index_row(i);
		auto rj = index_row(j);
		addmul(
			reinterpret_cast<uint8_t*>(rj.begin()), reinterpret_cast<uint8_t*>(rj.end()),
			reinterpret_cast<const uint8_t*>(ri.begin()), uint8_t(val)
		);
	}

	// NOLINTEND(cppcoreguidelines-pro-type-reinterpret-cast)

	// in place invert. the output is put into a and *this is turned into the
	// identity matrix. a is expected to be the identity matrix.
	void invert_with(GFMat& a) {
		for (int i = 0; i < r; i++) {
			int p_row = i;
			auto p_val = get(i, i);
			for (int j = i + 1; j < r && p_val.is_zero(); j++) {
				p_row = j;
				p_val = get(j, i);
			}
			if (p_val.is_zero()) {
				continue;
			}

			if (p_row != i) {
				swap_row(i, p_row);
				a.swap_row(i, p_row);
			}

			auto inv = p_val.inv();
			scale_row(i, inv);
			a.scale_row(i, inv);

			for (int j = i + 1; j < r; j++) {
				auto leading = get(j, i);
				addmul_row(i, j, leading);
				a.addmul_row(i, j, leading);
			}
		}

		for (int i = r - 1; i > 0; i--) {
			for (int j = i - 1; j >= 0; j--) {
				auto trailing = get(j, i);
				addmul_row(i, j, trailing);
				a.addmul_row(i, j, trailing);
			}
		}
	}

	// in place standardize.
	void standardize() {
		for (int i = 0; i < r; i++) {
			int p_row = i;
			auto p_val = get(i, i);
			for (int j = i + 1; j < r && p_val.is_zero(); j++) {
				p_row = j;
				p_val = get(j, i);
			}
			if (p_val.is_zero()) {
				continue;
			}

			if (p_row != i) {
				swap_row(i, p_row);
			}

			auto inv = p_val.inv();
			scale_row(i, inv);

			for (int j = i + 1; j < r; j++) {
				auto leading = get(j, i);
				addmul_row(i, j, leading);
			}
		}

		for (int i = r - 1; i > 0; i--) {
			for (int j = i - 1; j >= 0; j--) {
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
		for (int i = 0; i < c-r; i++) {
			out.set(i, i+r, 1);
		}

		// step 2: fill in the transposed P matrix. i and j are in terms of out.
		for (int i = 0; i < c-r; i++) {
			for (int j = 0; j < r; j++) {
				out.set(i, j, get(j, i+r));
			}
		}

		return out;
	}

	[[nodiscard]] auto get_r() const -> int {
		return r;
	}

	[[nodiscard]] auto get_c() const -> int {
		return c;
	}

private:
	GFVals d;
	int r;
	int c;
};

} // namespace infectious

#endif // INFECTIOUS_GF_ALG_HPP
