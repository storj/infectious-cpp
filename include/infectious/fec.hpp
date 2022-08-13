// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_FEC_HPP
#define INFECTIOUS_FEC_HPP

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "infectious/slice.hpp"

namespace infectious {

using namespace std::literals;

class TooManyErrors : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;

	TooManyErrors()
		: TooManyErrors("too many errors to reconstruct")
	{}
};

class NotEnoughShares : public std::out_of_range {
public:
	using std::out_of_range::out_of_range;

	NotEnoughShares()
		: NotEnoughShares("not enough shares")
	{}
};

using ShareOutputFunc = std::function<void(int num, const uint8_t* begin, const uint8_t* end)>;

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
		initialize();
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
	// this FEC is configured for. It will call the output callback n times.
	//
	// The input data must be a multiple of the required number of pieces k.
	// Padding to this multiple is up to the caller.
	//
	// Note that the byte slices in Shares passed to output may be reused when
	// output returns.
	void Encode(const uint8_t* ibegin, const uint8_t* iend, const ShareOutputFunc& output) const {
		int size = iend - ibegin;

		if (size < 0 || size % k != 0) {
			throw std::invalid_argument("input length must be a multiple of k");
		}

		int block_size = size / k;

		for (int i = 0; i < k; i++) {
			output(i, ibegin + i*block_size, ibegin + (i+1)*block_size);
		}

		std::vector<uint8_t> fec_buf(block_size);
		for (int i = k; i < n; i++) {
			std::fill(fec_buf.begin(), fec_buf.end(), 0);

			for (int j = 0; j < k; j++) {
				addmul(
					fec_buf.data(), fec_buf.data() + block_size,
					ibegin + j*block_size,
					enc_matrix[i*k+j]
				);
			}

			output(i, fec_buf.data(), fec_buf.data() + block_size);
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
		int num,
		const uint8_t* ibegin, const uint8_t* iend,
		uint8_t* obegin, uint8_t* oend
	) const {
		int isize = iend - ibegin;

		if (num < 0) {
			throw std::invalid_argument("num must be non-negative");
		}
		if (num >= n) {
			throw std::invalid_argument("num must be less than n");
		}
		if (isize % k != 0) {
			throw std::invalid_argument("input length must be a multiple of k");
		}

		int block_size = isize / k;

		int osize = static_cast<int>(oend - obegin);
		if (osize != block_size) {
			throw std::invalid_argument("output length must be equal to "s + std::to_string(block_size));
		}

		if (num < k) {
			std::copy(ibegin + num*block_size, ibegin + (num+1)*block_size, obegin);
			return;
		}

		std::fill(obegin, oend, 0);

		for (int i = 0; i < k; i++) {
			addmul(
				obegin, oend,
				ibegin + i*block_size,
				enc_matrix[num*k+i]
			);
		}
	}

	// Rebuild will take a list of corrected shares (pieces) and a callback
	// output. output will be called k times (FEC::Required() times) with 1/k
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
	void Rebuild(const std::map<int, uint8_t*>& shares, int share_size, const ShareOutputFunc& output) const {
		if (static_cast<int>(shares.size()) < k) {
			throw NotEnoughShares();
		}

		std::vector<uint8_t> decoding_matrix(k*k);
		std::vector<int> indexes(k);
		std::vector<uint8_t*> shares_begins(k);

		auto shares_b_iter = shares.begin();
		auto shares_e_iter = shares.rbegin();

		bool missing_primary_share = false;

		for (int i = 0; i < k; i++) {
			int share_id = 0;
			uint8_t* share_data = nullptr;
			if (shares_b_iter->first == i) {
				share_id = shares_b_iter->first;
				share_data = shares_b_iter->second;
				++shares_b_iter;
			} else {
				share_id = shares_e_iter->first;
				share_data = shares_e_iter->second;
				++shares_e_iter;
				missing_primary_share = true;
			}

			if (share_id < 0 || share_id >= n) {
				throw std::invalid_argument("invalid share id: "s + std::to_string(share_id));
			}

			if (share_id < k) {
				decoding_matrix[i*(k+1)] = 1;
				output(share_id, share_data, share_data + share_size);
			} else {
				// decode this after inverting the decoding matrix
				std::copy(&enc_matrix[share_id*k], &enc_matrix[(share_id+1)*k], &decoding_matrix[i*k]);
			}

			shares_begins[i] = share_data;
			indexes[i] = share_id;
		}

		// shortcut: if we have all the original data shares, we don't need to
		// perform a matrix inversion or do anything else.
		if (!missing_primary_share) {
			return;
		}

		invertMatrix(decoding_matrix, k);

		std::vector<uint8_t> buf(share_size);
		for (int i = 0; i < int(indexes.size()); ++i) {
			if (indexes[i] >= k) {
				std::fill(buf.begin(), buf.end(), 0);

				for (int col = 0; col < k; ++col) {
					addmul(buf.data(), buf.data() + share_size, shares_begins[col], decoding_matrix[i*k+col]);
				}
				output(i, buf.data(), buf.data() + share_size);
			}
		}
	}

	// Decode will take a destination buffer and a list of shares (pieces). It
	// will return the number of bytes written to the destination buffer.
	//
	// It will first correct the shares using Correct, mutating and reordering
	// the passed-in shares arguments. Then it will rebuild the data using
	// Rebuild. Finally it will concatenate the data into the given output
	// buffer.
	//
	// If you already know your data does not contain errors, Rebuild will be
	// faster.
	//
	// If you only want to identify which pieces are bad, you may be interested
	// in Correct.
	//
	// If you don't want the data concatenated for you, you can use Correct and
	// then Rebuild individually.
	auto Decode(const std::map<int, uint8_t*>& shares, int share_size, uint8_t* obegin, uint8_t* oend) const -> int {
		Correct(shares, share_size);
		int result_len = share_size * k;
		if ((oend - obegin) < result_len) {
			throw std::invalid_argument("output buffer must have at least "s + std::to_string(result_len) + " bytes available"s);
		}

		Rebuild(shares, share_size, [&](int num, const uint8_t* begin, const uint8_t* end) {
			std::copy(begin, end, obegin + num*share_size);
		});

		return result_len;
	}

	void DecodeTo(const std::map<int, uint8_t*>& shares, int share_size, const ShareOutputFunc& output) const {
		Correct(shares, share_size);
		Rebuild(shares, share_size, output);
	}

	// Correct implements the Berlekamp-Welch algorithm for correcting
	// errors in given FEC encoded data. It will correct the supplied shares,
	// mutating the underlying byte slices and reordering the shares
	void Correct(const std::map<int, uint8_t*>& shares, int share_size) const;

protected:
	[[nodiscard]] auto berlekampWelch(const std::vector<uint8_t*>& shares_vec, const std::vector<int>& shares_nums, int index) const -> std::vector<uint8_t>;

private:
	class GFMat;

	[[nodiscard]] auto syndromeMatrix(const std::vector<int>& shares_nums) const -> GFMat;

	void initialize();

	static void invertMatrix(std::vector<uint8_t>& matrix, int k);
	static void createInvertedVdm(std::vector<uint8_t>& vdm, int k);

	static void addmul(
		uint8_t* z_begin, const uint8_t* z_end,
		const uint8_t* x, uint8_t y
	);

	int k;
	int n;
	std::vector<uint8_t> enc_matrix;
	std::vector<uint8_t> vand_matrix;
};

} // namespace infectious

#endif // INFECTIOUS_FEC_HPP
