// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include "infectious/fec.hpp"
#include "gf_alg.hpp"

namespace infectious {

namespace {

const uint8_t interp_base = 2;

auto eval_point(int num) -> uint8_t {
	if (num == 0) {
		return 0;
	}
	return gf_pow(interp_base, num - 1);
}

} // namespace

void FEC::correct_(std::vector<uint8_t*>& shares_vec, std::vector<int>& shares_nums, long share_size) const {
	// fast path: check to see if there are no errors by evaluating it with
	// the syndrome matrix.
	auto synd = syndromeMatrix(shares_nums);
	std::vector<uint8_t> buf(share_size);

	for (int i = 0; i < synd.get_r(); i++) {
		std::fill(buf.begin(), buf.end(), 0);

		for (int j = 0; j < synd.get_c(); j++) {
			addmul(buf.data(), buf.data() + share_size, shares_vec[j], static_cast<uint8_t>(synd.get(i, j)));
		}

		for (int j = 0; j < static_cast<int>(buf.size()); j++) {
			if (buf[j] == 0) {
				continue;
			}
			auto data = berlekampWelch(shares_vec, shares_nums, j);
			for (int k = 0; k < static_cast<int>(shares_vec.size()); ++k) {
				shares_vec[k][j] = data[shares_nums[k]];
			}
		}
	}
}

auto FEC::berlekampWelch(const std::vector<uint8_t*>& shares_vec, const std::vector<int>& shares_nums, int index) const -> std::vector<uint8_t> {
	auto r = static_cast<int>(shares_vec.size()); // required + redundancy size
	auto e = (r - k) / 2;   // deg of E polynomial
	auto q = e + k;         // def of Q polynomial

	if (e <= 0) {
		throw NotEnoughShares();
	}

	auto dim = q + e;

	// build the system of equations s * u = f
	GFMat s(dim, dim);           // constraint matrix
	GFMat a(dim, dim);           // augmented matrix
	std::vector<uint8_t> f(dim); // constant column vector
	std::vector<uint8_t> u(dim); // solution vector

	for (int i = 0; i < dim; i++) {
		auto x_i = eval_point(shares_nums[i]);
		auto r_i = shares_vec[i][index];
		f[i] = gf_mul(gf_pow(x_i, e), r_i);

		for (int j = 0; j < q; j++) {
			s.set(i, j, gf_pow(x_i, j));
			if (i == j) {
				a.set(i, j, 1);
			}
		}

		for (int k = 0; k < e; k++) {
			auto j = k + q;

			s.set(i, j, gf_mul(gf_pow(x_i, k), r_i));
			if (i == j) {
				a.set(i, j, 1);
			}
		}
	}

	// invert and put the result in a
	s.invert_with(a);

	// multiply the inverted matrix by the column vector
	for (int i = 0; i < dim; i++) {
		auto *ri = a.index_row(i);
		u[i] = gf_dot(f, ri);
	}

	// reverse u for easier construction of the polynomials
	std::reverse(u.begin(), u.end());

	GFPoly q_poly(u);
	q_poly.shift(e);
	GFPoly e_poly(e+1);
	e_poly[0] = 1;
	std::copy(u.begin(), u.begin() + e, &e_poly[1]);

	auto [p_poly, rem] = q_poly.div(e_poly);
	if (!rem.is_zero()) {
		throw TooManyErrors();
	}

	std::vector<uint8_t> out(n);
	for (int i = 0; i < n; i++) {
		uint8_t pt = 0;
		if (i != 0) {
			pt = gf_pow(interp_base, i - 1);
		}
		out[i] = p_poly.eval(pt);
	}

	return out;
}

auto FEC::syndromeMatrix(const std::vector<int>& shares_nums) const -> GFMat {
	// get a list of keepers
	std::vector<bool> keepers(n);
	int shareCount = 0;
	for (auto share_num : shares_nums) {
		if (!keepers[share_num]) {
			keepers[share_num] = true;
			++shareCount;
		}
	}

	// create a vandermonde matrix but skip columns where we're missing the
	// share.
	GFMat out(k, shareCount);
	for (int i = 0; i < k; i++) {
		int skipped = 0;
		for (int j = 0; j < n; j++) {
			if (!keepers[j]) {
				skipped++;
				continue;
			}

			out.set(i, j-skipped, vand_matrix[i*n+j]);
		}
	}

	// standardize the output and convert into parity form
	out.standardize();
	return out.parity();
}

} // namespace infectious
