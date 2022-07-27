// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_FEC_HPP
#define INFECTIOUS_FEC_HPP

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "addmul.hpp"
#include "common.hpp"
#include "gf_alg.hpp"
#include "math.hpp"
#include "slice.hpp"
#include "tables.hpp"

namespace infectious {

using namespace std::literals;

struct Share {
	int num {0};
	Slice<uint8_t> data;
};

using ShareOutputFunc = std::function<void(int num, const Slice<uint8_t> data)>;

// we go without bounds checking on accesses to the gf_mul_table
// and its friends.
// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

// FEC represents operations performed on a Reed-Solomon-based
// forward error correction code.
class FEC {
public:
	static const int byte_max = 256;

	// This constructor creates a FEC using k required pieces and n total
	// pieces. Encoding data with this FEC will generate n pieces, and decoding
	// data requires k uncorrupted pieces. If during decode more than k pieces
	// exist, corrupted data can be detected and recovered from.
	FEC(int k_, int n_)
		: k {k_}
		, n {n_}
		, enc_matrix(n*k, 0)
		, vand_matrix(k*n, 0)
	{
		if (k <= 0 || n <= 0 || k > byte_max || n > byte_max || k > n) {
			throw std::domain_error("requires 1 <= k <= n <= 256");
		}

		Slice<uint8_t> temp_matrix(n*k, 0);
		createInvertedVdm(temp_matrix, k);

		for (int i = k * k; i < temp_matrix.size(); i++) {
			temp_matrix[i] = gf_exp[((i/k)*(i%k))%(byte_max-1)];
		}

		for (int i = 0; i < k; i++) {
			enc_matrix[i*(k+1)] = 1;
		}

		for (int row = k * k; row < n*k; row += k) {
			for (int col = 0; col < k; col++) {
				auto pa = temp_matrix.slice(row);
				auto pb = temp_matrix.slice(col);
				uint8_t acc {0};
				for (int i = 0; i < k; i++, pa = pa.slice(1), pb = pb.slice(k)) {
					acc ^= gf_mul_table[pa[0]][pb[0]];
				}
				enc_matrix[row+col] = acc;
			}
		}

		// vand_matrix has more columns than rows
		// k rows, n columns.
		vand_matrix[0] = 1;
		uint8_t g {1};
		for (int row = 0; row < k; row++) {
			uint8_t a {1};
			for (int col = 1; col < n; col++) {
				vand_matrix[row*n+col] = a; // 2.pow(i * j) FIGURE IT OUT
				a = gf_mul_table[g][a];
			}
			g = gf_mul_table[2][g];
		}
	}

	// Required returns the number of required pieces for reconstruction. This
	// is the k value passed to NewFEC.
	[[nodiscard]] auto Required() const -> int {
		return k;
	}

	// Total returns the number of total pieces that will be generated during
	// encoding. This is the n value passed to NewFEC.
	[[nodiscard]] auto Total() const -> int {
		return n;
	}

	// Encode will take input data and encode to the total number of pieces n
	// this FEC is configured for. It will call the callback output n times.
	//
	// The input data must be a multiple of the required number of pieces k.
	// Padding to this multiple is up to the caller.
	//
	// Note that the byte slices in Shares passed to output may be reused when
	// output returns.
	void Encode(const Slice<uint8_t>& input, const ShareOutputFunc& output) const {
		int size = input.size();

		if (size < 0 || size % k != 0) {
			throw std::invalid_argument("input length must be a multiple of k");
		}

		int block_size = size / k;

		for (int i = 0; i < k; i++) {
			output(i, input.slice(i*block_size, i*block_size+block_size));
		}

		Slice<uint8_t> fec_buf(block_size);
		for (int i = k; i < n; i++) {
			std::fill(fec_buf.begin(), fec_buf.end(), 0);

			for (int j = 0; j < k; j++) {
				addmul(
					fec_buf.begin(), fec_buf.end(),
					&input[j*block_size],
					enc_matrix[i*k+j]
				);
			}

			output(i, fec_buf);
		}
	}

	// EncodeSingle will take input data and encode it to output only for the
	// num piece.
	//
	// The input data must be a multiple of the required number of pieces k.
	// Padding to this multiple is up to the caller.
	//
	// The output must be exactly len(input) / k bytes.
	//
	// The num must be 0 <= num < n.
	void EncodeSingle(
		const Slice<uint8_t>& input,
		uint8_t* output_begin, uint8_t* output_end,
		int num
	) const {
		int size = input.size();

		if (num < 0) {
			throw std::invalid_argument("num must be non-negative");
		}

		if (num >= n) {
			throw std::invalid_argument("num must be less than n");
		}

		if (size % k != 0) {
			throw std::invalid_argument("input length must be a multiple of k");
		}

		int block_size = size / k;

		int output_size = static_cast<int>(output_end - output_begin);
		if (output_size != block_size) {
			throw std::invalid_argument("output length must be equal to "s + std::to_string(block_size));
		}

		if (num < k) {
			std::copy(&input[num*block_size], &input[num*block_size + block_size], output_begin);
			return;
		}

		std::fill(output_begin, output_end, 0);

		for (int i = 0; i < k; i++) {
			addmul(
				output_begin, output_end,
				&input[i*block_size],
				enc_matrix[num*k+i]
			);
		}
	}

	// Rebuild will take a list of corrected shares (pieces) and a callback
	// output. output will be called k times ((*FEC).Required() times) with 1/k
	// of the original data each time and the index of that data piece. Decode
	// is usually preferred.
	//
	// Note that the data is not necessarily sent to output ordered by the piece
	// number.
	//
	// Note that the byte slices in Shares passed to output may be reused when
	// output returns.
	//
	// Rebuild assumes that you have already called Correct or did not need to.
	void Rebuild(std::vector<Share>& shares, const ShareOutputFunc& output) const {
		if (static_cast<int>(shares.size()) < k) {
			throw NotEnoughShares();
		}

		auto share_size = shares[0].data.size();
		std::sort(shares.begin(), shares.end(), [](const Share& a, const Share& b) {
			return a.num < b.num;
		});

		Slice<uint8_t> m_dec(k*k);
		std::vector<int> indexes(k);
		std::vector<uint8_t*> shares_begins(k);

		int shares_b_iter = 0;
		int shares_e_iter = static_cast<int>(shares.size()) - 1;

		for (int i = 0; i < k; i++) {
			auto share = shares[shares_b_iter];
			if (share.num == i) {
				shares_b_iter++;
			} else {
				share = shares[shares_e_iter];
				shares_e_iter--;
			}
			int share_id = share.num;

			if (share_id >= n) {
				throw std::invalid_argument("invalid share id: "s + std::to_string(share_id));
			}

			if (share_id < k) {
				m_dec[i*(k+1)] = 1;
				if (output) {
					output(share_id, share.data);
				}
			} else {
				std::copy(&enc_matrix[share_id*k], &enc_matrix[share_id*k+k], &m_dec[i*k]);
			}

			shares_begins[i] = share.data.begin();
			indexes[i] = share_id;
		}

		invertMatrix(m_dec, k);

		Slice<uint8_t> buf(share_size);
		for (int i = 0; i < int(indexes.size()); i++) {
			if (indexes[i] >= k) {
				std::fill(buf.begin(), buf.end(), 0);

				for (int col = 0; col < k; col++) {
					addmul(buf.begin(), buf.end(), shares_begins[col], m_dec[i*k+col]);
				}

				if (output) {
					output(i, buf);
				}
			}
		}
	}

	// Decode will take a destination buffer and a list of shares (pieces). It
	// will return the number of bytes written to the destination buffer.
	//
	// It will first correct the shares using Correct, mutating and reordering
	// the passed-in shares arguments. Then it will rebuild the data using
	// Rebuild. Finally it will concatenate the data into the given output
	// buffer dst.
	//
	// If you already know your data does not contain errors, Rebuild will be
	// faster.
	//
	// If you only want to identify which pieces are bad, you may be interested
	// in Correct.
	//
	// If you don't want the data concatenated for you, you can use Correct and
	// then Rebuild individually.
	auto Decode(Slice<uint8_t>& dst, std::vector<Share>& shares) const -> int;

	void DecodeTo(std::vector<Share>& shares, const ShareOutputFunc& output) const;

	// Correct implements the Berlekamp-Welch algorithm for correcting
	// errors in given FEC encoded data. It will correct the supplied shares,
	// mutating the underlying byte slices and reordering the shares
	void Correct(std::vector<Share>& shares) const;

protected:
	[[nodiscard]] auto berlekampWelch(const std::vector<Share>& shares, int index) const -> Slice<uint8_t>;
	[[nodiscard]] auto syndromeMatrix(const std::vector<Share>& shares) const -> GFMat;

private:
	int k;
	int n;
	Slice<uint8_t> enc_matrix;
	Slice<uint8_t> vand_matrix;
};

// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)

} // namespace infectious

#endif // INFECTIOUS_FEC_HPP
