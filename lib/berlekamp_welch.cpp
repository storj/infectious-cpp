// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include "common.hpp"
#include "fec.hpp"
#include "math.hpp"

namespace infectious {

int FEC::Decode(Slice<uint8_t>& dst, std::vector<Share>& shares) const {
	Correct(shares);
	if (shares.size() == 0) {
		throw std::invalid_argument("must specify at least one share");
	}
	int piece_len = shares[0].size();
	int result_len = piece_len * k;
	if (dst.size() < result_len) {
		throw std::invalid_argument("dst buffer must have at least "s + std::to_string(result_len) + " bytes available"s);
	}

	Rebuild(shares, [&](int num, const Slice<uint8_t>& output) {
		std::copy(output.begin(), output.end(), &dst[num*piece_len]);
	});

	return result_len;
}

void FEC::DecodeTo(std::vector<Share>& shares, const ShareOutputFunc& output) const {
	Correct(shares);
	Rebuild(shares, output);
}

void FEC::Correct(std::vector<Share>& shares) const {
	if (static_cast<int>(shares.size()) < k) {
		throw std::invalid_argument("must specify at least the number of required shares");
	}

	std::sort(shares.begin(), shares.end(), [](const Share& a, const Share& b) {
		return a.num < b.num;
	});

	// fast path: check to see if there are no errors by evaluating it with
	// the syndrome matrix.
	auto synd = syndromeMatrix(shares);
	Slice<uint8_t> buf(shares[0].size());

	for (int i = 0; i < synd.get_r(); i++) {
		std::fill(buf.begin(), buf.end(), 0);

		for (int j = 0; j < synd.get_c(); j++) {
			addmul(buf.begin(), buf.end(), shares[j].data.begin(), static_cast<uint8_t>(synd.get(i, j)));
		}

		for (int j = 0; j < buf.size(); j++) {
			if (buf[j] == 0) {
				continue;
			}
			auto data = berlekampWelch(shares, j);
			for (auto& share : shares) {
				share.data[j] = data[share.num];
			}
		}
	}
}

Slice<uint8_t> FEC::berlekampWelch(const std::vector<Share>& shares, int index) const {
	auto r = shares.size(); // required + redundancy size
	auto e = (r - k) / 2;   // deg of E polynomial
	auto q = e + k;         // def of Q polynomial

	if (e <= 0) {
		throw NotEnoughShares();
	}

	const GFVal interp_base{2};

	auto eval_point = [&](int num) -> GFVal {
		if (num == 0) {
			return 0;
		}
		return interp_base.pow(num - 1);
	};

	auto dim = q + e;

	// build the system of equations s * u = f
	GFMat s(dim, dim); // constraint matrix
	GFMat a(dim, dim); // augmented matrix
	GFVals f(dim);     // constant column vector
	GFVals u(dim);     // solution vector

	for (unsigned int i = 0; i < dim; i++) {
		auto x_i = eval_point(shares[i].num);
		auto r_i = shares[i].data[index];
		f[i] = x_i.pow(e).mul(r_i);

		for (unsigned int j = 0; j < q; j++) {
			s.set(i, j, x_i.pow(j));
			if (i == j) {
				a.set(i, j, 1);
			}
		}

		for (unsigned int k = 0; k < e; k++) {
			auto j = k + q;

			s.set(i, j, x_i.pow(k).mul(r_i));
			if (i == j) {
				a.set(i, j, 1);
			}
		}
	}

	// invert and put the result in a
	s.invert_with(a);

	// multiply the inverted matrix by the column vector
	for (unsigned int i = 0; i < dim; i++) {
		auto ri = a.index_row(i);
		u[i] = ri.dot(f);
	}

	// reverse u for easier construction of the polynomials
	std::reverse(u.begin(), u.end());

	GFPoly q_poly(u.slice(e));
	GFPoly e_poly(e+1);
	e_poly[0] = 1;
	std::copy(u.begin(), u.begin() + e, &e_poly[1]);

	auto [p_poly, rem] = q_poly.div(e_poly);
	if (!rem.is_zero()) {
		throw TooManyErrors();
	}

	Slice<uint8_t> out(n);
	for (int i = 0; i < n; i++) {
		GFVal pt = 0;
		if (i != 0) {
			pt = interp_base.pow(i - 1);
		}
		out[i] = static_cast<uint8_t>(p_poly.eval(pt));
	}

	return out;
}

GFMat FEC::syndromeMatrix(const std::vector<Share>& shares) const {
	// get a list of keepers
	std::vector<bool> keepers(n);
	int shareCount = 0;
	for (auto& share : shares) {
		if (!keepers[share.num]) {
			keepers[share.num] = true;
			shareCount++;
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

			out.set(i, j-skipped, GFVal(vand_matrix[i*n+j]));
		}
	}

	// standardize the output and convert into parity form
	out.standardize();
	return out.parity();
}

} // namespace infectious
