// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include <utility>

#include "addmul.hpp"
#include "math.hpp"
#include "tables.hpp"

namespace infectious {

struct pivotSearcher {
	pivotSearcher(int k_)
		: k {k_}
		, ipiv(k)
	{}

	std::pair<int, int> search(int col, const Slice<uint8_t>& matrix) {
		if (!ipiv[col] && matrix[col*k+col] != 0) {
			ipiv[col] = true;
			return std::make_pair(col, col);
		}

		for (int row = 0; row < k; row++) {
			if (ipiv[row]) {
				continue;
			}

			for (int i = 0; i < k; i++) {
				if (!ipiv[i] && matrix[row*k+i] != 0) {
					ipiv[i] = true;
					return std::make_pair(row, i);
				}
			}
		}

		throw std::invalid_argument("pivot not found");
	}

	int k;
	std::vector<bool> ipiv;
};

// TODO(jeff): matrix is a K*K array, row major.
void invertMatrix(Slice<uint8_t>& matrix, int k) {
	pivotSearcher pivot_searcher(k);
	std::vector<int> indxc(k);
	std::vector<int> indxr(k);
	Slice<uint8_t> id_row(k);

	for (int col = 0; col < k; col++) {
		auto [icol, irow] = pivot_searcher.search(col, matrix);

		if (irow != icol) {
			for (int i = 0; i < k; i++) {
				std::swap(matrix[irow*k+i], matrix[icol*k+i]);
			}
		}

		indxr[col] = irow;
		indxc[col] = icol;
		auto pivot_row = matrix.slice(icol*k, icol*k+k);
		auto c = pivot_row[icol];

		if (c == 0) {
			throw std::domain_error("singular matrix");
		}

		if (c != 1) {
			c = gf_inverse[c];
			pivot_row[icol] = 1;
			const auto& mul_c = gf_mul_table[c];

			for (int i = 0; i < k; i++) {
				pivot_row[i] = mul_c[pivot_row[i]];
			}
		}

		id_row[icol] = 1;
		if (memcmp(pivot_row.begin(), id_row.begin(), pivot_row.size()) != 0) {
			auto p = matrix;
			for (int i = 0; i < k; i++) {
				if (i != icol) {
					c = p[icol];
					p[icol] = 0;
					addmul(&p[0], &p[k], pivot_row.begin(), c);
				}
				p = p.slice(k);
			}
		}

		id_row[icol] = 0;
	}

	for (int i = 0; i < k; i++) {
		if (indxr[i] != indxc[i]) {
			for (int row = 0; row < k; row++) {
				std::swap(matrix[row*k+indxr[i]], matrix[row*k+indxc[i]]);
			}
		}
	}
}

void createInvertedVdm(Slice<uint8_t>& vdm, int k) {
	if (k == 1) {
		vdm[0] = 1;
		return;
	}

	Slice<uint8_t> b(k);
	Slice<uint8_t> c(k);

	c[k-1] = 0;
	for (int i = 1; i < k; i++) {
		auto& mul_p_i = gf_mul_table[gf_exp[i]];
		for (int j = k - 1 - (i - 1); j < k-1; j++) {
			c[j] ^= mul_p_i[c[j+1]];
		}
		c[k-1] ^= gf_exp[i];
	}

	for (int row = 0; row < k; row++) {
		int index = 0;
		if (row != 0) {
			index = static_cast<int>(gf_exp[row]);
		}
		auto& mul_p_row = gf_mul_table[index];

		uint8_t t = 1;
		b[k-1] = 1;
		for (int i = k - 2; i >= 0; i--) {
			b[i] = c[i+1] ^ mul_p_row[b[i+1]];
			t = b[i] ^ mul_p_row[t];
		}

		auto& mul_t_inv = gf_mul_table[gf_inverse[t]];
		for (int col = 0; col < k; col++) {
			vdm[col*k+row] = mul_t_inv[b[col]];
		}
	}
}

} // namespace infectious
