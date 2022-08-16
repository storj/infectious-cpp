// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_FEC_HPP
#define INFECTIOUS_FEC_HPP

#include <algorithm>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "infectious/build_env.h"

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

using ByteView = std::basic_string_view<uint8_t>;

using ShareOutputFunc = std::function<void(int num, ByteView output)>;

namespace {
	template <typename T, typename = void>
	struct is_sortable : std::false_type {};

	template <typename T>
	struct is_sortable<T, decltype(std::sort(std::declval<T&>()))> : std::true_type {};

	template <typename T>
	inline constexpr bool is_sortable_v = is_sortable<T>::value;
}

// share_num and share_data can be overloaded as needed to extract share number
// and share data information from a ShareMap iterator.

template <typename T>
auto share_num(const T& t) {
	return t.first;
}

template <typename T>
auto share_data(T& t) -> decltype(t.second)& {
	return t.second;
}

template <typename T>
auto share_data(const T& t) -> const decltype(t.second)& {
	return t.second;
}

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
	// Note that the byte ranges in Shares passed to output may be reused when
	// output returns.
	template <typename InputType>
	void Encode(const InputType& input, const ShareOutputFunc& output) const {
		int size = input.size();

		if (size < 0 || size % k != 0) {
			throw std::invalid_argument("input length must be a multiple of k");
		}

		int block_size = size / k;

		auto ibegin = std::begin(input);
		for (int i = 0; i < k; i++) {
			output(i, ByteView(std::to_address(ibegin) + i*block_size, block_size));
		}

		std::vector<uint8_t> fec_buf(block_size);
		for (int i = k; i < n; i++) {
			std::fill(fec_buf.begin(), fec_buf.end(), 0);

			for (int j = 0; j < k; j++) {
				addmul(
					fec_buf.data(), fec_buf.data() + block_size,
					std::to_address(ibegin) + j*block_size,
					enc_matrix[i*k+j]
				);
			}

			output(i, ByteView(fec_buf.data(), block_size));
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
	template <typename IBegin, typename IEnd, typename OBegin, typename OEnd>
	void EncodeSingle(
		int num,
		const IBegin ibegin, IEnd iend,
		OBegin obegin, OEnd oend
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
				std::to_address(obegin), std::to_address(oend),
				std::to_address(ibegin) + i*block_size,
				enc_matrix[num*k+i]
			);
		}
	}

	// RebuildSorted will take a list of corrected, sorted shares (pieces) and a
	// callback output. output will be called k times (FEC::Required() times)
	// with 1/k of the original data each time and the index of that data piece.
	// Decode is usually preferred.
	//
	// Note that the data is not necessarily sent to output ordered by the piece
	// number.
	//
	// Note that the byte ranges in Shares passed to output may be reused when
	// output returns.
	//
	// RebuildSorted assumes that you have already called Correct or did not
	// need to, and that the iterator will yield shares in order of their share
	// numbers.
	template <typename ShareMap>
	void RebuildSorted(const ShareMap& shares, const ShareOutputFunc& output) const {
		if (static_cast<int>(shares.size()) < k) {
			throw NotEnoughShares();
		}

		std::vector<uint8_t> decoding_matrix(k*k);
		std::vector<int> indexes(k);
		std::vector<const uint8_t*> shares_begins(k);

		auto shares_b_iter = std::begin(shares);
		auto shares_e_iter = std::rbegin(shares);

		bool missing_primary_share = false;
		int share_size = 0;

		for (int i = 0; i < k; i++) {
			int share_id = 0;
			int shares_b_num = share_num(*shares_b_iter);
			if (shares_b_num == i) {
				share_id = shares_b_num;
				auto& data = share_data(*shares_b_iter);
				shares_begins[i] = std::to_address(std::begin(data));
				share_size = std::to_address(std::end(data)) - shares_begins[i];
				++shares_b_iter;
			} else {
				share_id = share_num(*shares_e_iter);
				auto& data = share_data(*shares_e_iter);
				shares_begins[i] = std::to_address(std::begin(data));
				share_size = std::to_address(std::end(data)) - shares_begins[i];
				++shares_e_iter;
				missing_primary_share = true;
			}

			if (share_id < 0 || share_id >= n) {
				throw std::invalid_argument("invalid share id: "s + std::to_string(share_id));
			}

			if (share_id < k) {
				decoding_matrix[i*(k+1)] = 1;
				output(share_id, ByteView(shares_begins[i], share_size));
			} else {
				// decode this after inverting the decoding matrix
				std::copy(&enc_matrix[share_id*k], &enc_matrix[(share_id+1)*k], &decoding_matrix[i*k]);
			}

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
				output(i, ByteView(buf.data(), share_size));
			}
		}
	}

	// if shares isn't already a sorted map and we can't sort in-place,
	// we need to copy the elements into a new sorted map.
	template <typename ShareMap, typename = void>
	struct rebuild_specialization {
		using DataType = decltype(share_data(*std::declval<ShareMap>().begin()));

		static void SortAndRebuild(FEC* this_, ShareMap& shares, const ShareOutputFunc& output) {
			std::map<int, DataType> sorted_shares;
			for (auto& share : shares) {
				sorted_shares.try_emplace(share_num(share), share_data(share));
			}
			this_->RebuildSorted(sorted_shares, output);
		}
	};

	// if shares is sortable in-place, we will do that.
	template <typename ShareMap>
	struct rebuild_specialization<ShareMap, std::enable_if_t<is_sortable_v<ShareMap>>> {
		static void SortAndRebuild(FEC* this_, ShareMap& shares, const ShareOutputFunc& output) {
			std::sort(shares);
			this_->RebuildSorted(shares, output);
		}
	};

	// if shares is already a sorted map, we can pass it straight through.
	template <typename IntType, typename DataType>
	struct rebuild_specialization<std::map<IntType, DataType>> {
		static void SortAndRebuild(FEC* this_, const std::map<IntType, DataType>& shares, const ShareOutputFunc& output) {
			this_->RebuildSorted(shares, output);
		}
	};

	// Rebuild will take a list of corrected shares (pieces) and a callback
	// output. output will be called k times (FEC::Required() times) with 1/k
	// of the original data each time and the index of that data piece. Decode
	// is usually preferred.
	//
	// Note that the data is not necessarily sent to output ordered by the piece
	// number.
	//
	// Note that the byte ranges in Shares passed to output may be reused when
	// output returns.
	//
	// Rebuild assumes that you have already called Correct or did not need to.
	template <typename ShareMap>
	void Rebuild(ShareMap& shares, const ShareOutputFunc& output) {
		rebuild_specialization<ShareMap>::SortAndRebuild(this, shares, output);
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
	template <typename ShareMap, typename OutputType>
	auto Decode(ShareMap& shares, OutputType& output) -> int {
		Correct(shares);
		int share_size = 0;

		Rebuild(shares, [&](int num, const ByteView& rebuilt) {
			if (share_size == 0) {
				share_size = static_cast<int>(rebuilt.size());
				if (static_cast<int>(output.size()) < share_size * k) {
					throw std::invalid_argument("output buffer must have at least "s + std::to_string(share_size * k) + " bytes available"s);
				}
			}
			std::copy(rebuilt.begin(), rebuilt.end(), output.begin() + num*share_size);
		});

		return share_size * k;
	}

	template <typename ShareMap>
	void DecodeTo(ShareMap& shares, const ShareOutputFunc& output) {
		Correct(shares);
		Rebuild(shares, output);
	}

	// Correct implements the Berlekamp-Welch algorithm for correcting
	// errors in given FEC encoded data. It will correct the supplied shares,
	// mutating the underlying byte ranges and reordering the shares
	template <typename ShareMap>
	void Correct(ShareMap& shares) const {
		if (static_cast<int>(shares.size()) < k) {
			throw std::invalid_argument("must specify at least the number of required shares");
		}

		std::vector<uint8_t*> shares_vec(shares.size());
		std::vector<int> shares_nums(shares.size());
		shares_vec.resize(0);
		shares_nums.resize(0);
		int share_size = 0;
		for (auto& share : shares) {
			auto& v = share_data(share);
			uint8_t* data_start = std::to_address(std::begin(v));
			uint8_t* data_end = std::to_address(std::end(v));
			share_size = data_end - data_start;
			shares_vec.push_back(data_start);
			shares_nums.push_back(share_num(share));
		}

		correct_(shares_vec, shares_nums, share_size);
	}

protected:
	[[nodiscard]] auto berlekampWelch(const std::vector<uint8_t*>& shares_vec, const std::vector<int>& shares_nums, int index) const -> std::vector<uint8_t>;

private:
	class GFMat;

	void initialize();
	void correct_(std::vector<uint8_t*>& shares_vec, std::vector<int>& shares_nums, int share_size) const;
	[[nodiscard]] auto syndromeMatrix(const std::vector<int>& shares_nums) const -> GFMat;

	static void invertMatrix(std::vector<uint8_t>& matrix, int k);
	static void createInvertedVdm(std::vector<uint8_t>& vdm, int k);

	static void addmul(
		uint8_t* z_begin, const uint8_t* z_end,
		const uint8_t* x, uint8_t y
	);
#if defined(INFECTIOUS_HAS_VPERM)
	static size_t addmul_vperm(uint8_t z[], const uint8_t x[], uint8_t y, size_t size);
#endif
#if defined(INFECTIOUS_HAS_SSE2)
	static size_t addmul_sse2(uint8_t z[], const uint8_t x[], uint8_t y, size_t size);
#endif

	int k;
	int n;
	std::vector<uint8_t> enc_matrix;
	std::vector<uint8_t> vand_matrix;
};

auto addmul_provider() -> std::string;

auto build_environment() -> const char*;

} // namespace infectious

#endif // INFECTIOUS_FEC_HPP
