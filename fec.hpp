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
#include "gf_alg.hpp"
#include "math.hpp"
#include "slice.hpp"
#include "tables.hpp"

namespace infectious {

using namespace std::literals;

struct Share {
	int num;
	uint8_t* begin;
	uint8_t* end;

	int size() const {
		return end - begin;
	}
};

// FEC represents operations performed on a Reed-Solomon-based
// forward error correction code.
class FEC {
public:
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
		if (k <= 0 || n <= 0 || k > 256 || n > 256 || k > n) {
			throw std::domain_error("requires 1 <= k <= n <= 256");
		}

		Slice<uint8_t> temp_matrix(n*k, 0);
		createInvertedVdm(temp_matrix, k);

		for (int i = k * k; i < temp_matrix.size(); i++) {
			temp_matrix[i] = gf_exp[((i/k)*(i%k))%255];
		}

		for (int i = 0; i < k; i++) {
			enc_matrix[i*(k+1)] = 1;
		}

		for (int row = k * k; row < n*k; row += k) {
			for (int col = 0; col < k; col++) {
				auto pa = temp_matrix.subspan(row);
				auto pb = temp_matrix.subspan(col);
				uint8_t acc {0};
				for (int i = 0; i < k; i++, pa = pa.subspan(1), pb = pb.subspan(k)) {
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
	int Required() const {
		return k;
	}

	// Total returns the number of total pieces that will be generated during
	// encoding. This is the n value passed to NewFEC.
	int Total() const {
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
	void Encode(
		const uint8_t* input_begin, const uint8_t* input_end,
		const std::function<void(int num, const uint8_t* output_begin, const uint8_t* output_end)>& output
	) const {
		int size = input_end - input_begin;

		if (size < 0 || size % k != 0) {
			throw std::invalid_argument("input length must be a multiple of k");
		}

		int block_size = size / k;

		for (int i = 0; i < k; i++) {
			output(i, input_begin + (i*block_size), input_begin + (i*block_size+block_size));
		}

		Slice<uint8_t> fec_buf(block_size);
		for (int i = k; i < n; i++) {
			std::fill(fec_buf.begin(), fec_buf.end(), 0);

			for (int j = 0; j < k; j++) {
				addmul(
					fec_buf.begin(), fec_buf.end(),
					input_begin + (j*block_size),
					enc_matrix[i*k+j]
				);
			}

			output(i, fec_buf.begin(), fec_buf.end());
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
		const uint8_t* input_begin, const uint8_t* input_end,
		uint8_t* output_begin, uint8_t* output_end,
		int num) {
		int size = input_end - input_begin;

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

		int output_size = output_end - output_begin;
		if (output_size != block_size) {
			throw std::invalid_argument("output length must be equal to "s + std::to_string(block_size));
		}

		if (num < k) {
			std::copy(input_begin + (num*block_size), input_begin + (num*block_size + block_size), output_begin);
			return;
		}

		std::fill(output_begin, output_end, 0);

		for (int i = 0; i < k; i++) {
			addmul(
				output_begin, output_end,
				input_begin + (i*block_size),
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
	void Rebuild(
		std::vector<Share>& shares,
		const std::function<void(int num, const uint8_t* begin, const uint8_t* end)>& output
	) {
		if (static_cast<int>(shares.size()) < k) {
			throw NotEnoughShares();
		}

		auto share_size = shares[0].size();
		std::sort(shares.begin(), shares.end(), [](const Share& a, const Share& b) {
			return a.num < b.num;
		});

		Slice<uint8_t> m_dec(k*k);
		std::vector<int> indexes(k);
		std::vector<uint8_t*> shares_begins(k);
		std::vector<uint8_t*> shares_ends(k);

		int shares_b_iter = 0;
		int shares_e_iter = shares.size() - 1;

		for (int i = 0; i < k; i++) {
			int share_id;
			uint8_t* share_data_begin;
			uint8_t* share_data_end;

			auto share = shares[shares_b_iter];
			if (share.num == i) {
				shares_b_iter++;
			} else {
				share = shares[shares_e_iter];
				shares_e_iter--;
			}
			share_id = share.num;
			share_data_begin = share.begin;
			share_data_end = share.end;

			if (share_id >= n) {
				throw std::invalid_argument("invalid share id: "s + std::to_string(share_id));
			}

			if (share_id < k) {
				m_dec[i*(k+1)] = 1;
				if (output) {
					output(share_id, share_data_begin, share_data_end);
				}
			} else {
				std::copy(&enc_matrix[share_id*k], &enc_matrix[share_id*k+k], &m_dec[i*k]);
			}

			shares_begins[i] = share_data_begin;
			shares_ends[i] = share_data_end;
			indexes[i] = share_id;
		}

		invertMatrix(m_dec, k);

		Slice<uint8_t> buf(share_size);
		for (unsigned int i = 0; i < indexes.size(); i++) {
			if (indexes[i] >= k) {
				std::fill(buf.begin(), buf.end(), 0);

				for (int col = 0; col < k; col++) {
					addmul(buf.begin(), buf.end(), shares_begins[col], m_dec[i*k+col]);
				}

				if (output) {
					output(i, buf.begin(), buf.end());
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
	int Decode(Slice<uint8_t>& dst, std::vector<Share>& shares);

	void DecodeTo(
		std::vector<Share>& shares,
		const std::function<void(int num, const uint8_t* begin, const uint8_t* end)>& output
	);

	// Correct implements the Berlekamp-Welch algorithm for correcting
	// errors in given FEC encoded data. It will correct the supplied shares,
	// mutating the underlying byte slices and reordering the shares
	void Correct(std::vector<Share>& shares);

protected:
	Slice<uint8_t> berlekampWelch(const std::vector<Share>& shares, int index);
	GFMat syndromeMatrix(const std::vector<Share>& shares);

protected:
	int k;
	int n;
	Slice<uint8_t> enc_matrix;
	Slice<uint8_t> vand_matrix;
};

} // namespace infectious

#endif // INFECTIOUS_FEC_HPP
