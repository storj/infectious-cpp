#include <cinttypes>
#include <random>
#include <tuple>
#include <utility>
#include <vector>
#include "gtest/gtest.h"

#include "infectious/fec.hpp"

namespace infectious {
namespace test {

// for some reason, clang-tidy doesn't much care for gtest's macros.
// NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

std::random_device rd;
std::mt19937 generator(rd());

struct BerlekampWelchTest : public FEC {
public:
	using FEC::FEC;

	[[nodiscard]] auto StoreShares() const -> std::pair<std::shared_ptr<std::map<int, std::vector<uint8_t>>>, ShareOutputFunc> {
		auto out = std::make_shared<std::map<int, std::vector<uint8_t>>>();

		return std::make_pair(out, [out](int num, ByteView output) {
			(*out)[num].resize(output.size());
			std::copy(output.begin(), output.end(), (*out)[num].begin());
		});
	}

	[[nodiscard]] auto SomeShares(int block) const -> std::pair<std::vector<uint8_t>, std::map<int, std::vector<uint8_t>>> {
		// seed the initial data
		std::vector<uint8_t> data(Required() * block);

		for (int i = 0; i < static_cast<int>(data.size()); ++i) {
			data[i] = static_cast<uint8_t>(i+1);
		}

		auto [shares, store] = StoreShares();
		Encode(data, store);

		return std::make_pair(data, *shares);
	}

	[[nodiscard]] auto BerlekampWelch(const std::vector<uint8_t*>& shares, const std::vector<int>& shares_nums, int index) -> std::vector<uint8_t> {
		return berlekampWelch(shares, shares_nums, index);
	}

	[[nodiscard]] auto CopyShares(const std::map<int, std::vector<uint8_t>>& shares) -> std::map<int, std::vector<uint8_t>> {
		return shares;
	}

	static void MutateShare(int idx, std::pair<int, std::vector<uint8_t>>& share) {
		const int byte_limit = 256;
		auto orig = share.second[idx];
		auto next = static_cast<uint8_t>(randn(byte_limit));
		while (next == orig) {
			next = static_cast<uint8_t>(randn(byte_limit));
		}
		share.second[idx] = next;
	}

	void PermuteShares(std::vector<std::pair<int, std::vector<uint8_t>>>& shares) {
		for (unsigned int i = 0; i < shares.size(); i++) {
			auto with = randn(static_cast<int>(shares.size())-i) + i;
			std::swap(shares[i], shares[with]);
		}
	}

	std::map<int, std::vector<uint8_t>> as_map(std::vector<std::pair<int, std::vector<uint8_t>>>& m) {
		return std::map(m.begin(), m.end());
	}

	std::map<int, std::vector<uint8_t>> as_map(std::map<int, std::vector<uint8_t>>& m) {
		return m;
	}

	template <typename T1, typename T2>
	void AssertEqualShares(T1& expected, T2& got) {
		ASSERT_EQ(as_map(expected), as_map(got));
	}

	// can be used as a do-nothing output callback
	static void noop(int, ByteView) {}

	[[nodiscard]] static auto randn(int limit) -> int {
		std::uniform_int_distribution<> distrib(0, limit - 1);
		return distrib(generator);
	}
};

TEST(BerlekampWelch, SingleBlock) {
	const int block = 1;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto shares = test.SomeShares(block).second;

	std::vector<uint8_t*> shares_data;
	std::vector<int> shares_nums;
	for (int i = 0; i < static_cast<int>(shares.size()); ++i) {
		if (!shares[i].empty()) {
			shares_data.push_back(shares[i].data());
			shares_nums.push_back(i);
		}
	}
	auto out = test.BerlekampWelch(shares_data, shares_nums, 0);
	ASSERT_EQ(out, (std::vector<uint8_t>{0x01, 0x02, 0x03, 0x15, 0x69, 0xcc, 0xf2}));
}

TEST(BerlekampWelch, MultipleBlock) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto shares = test.SomeShares(block).second;

	test.DecodeTo(shares, BerlekampWelchTest::noop);

	shares[0][0]++;
	shares[1][0]++;

	auto [decoded_shares, callback] = test.StoreShares();
	test.DecodeTo(shares, callback);

	for (int i = required; i < total; ++i) {
		shares.erase(i);
	}
	test.AssertEqualShares(shares, *decoded_shares);
}

TEST(BerlekampWelch, TestDecode) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto [origdata, shares] = test.SomeShares(block);

	std::vector<uint8_t> output(origdata.size() + 1);
	int output_len = test.Decode(shares, output);
	output.resize(output_len);
	ASSERT_EQ(origdata, output);
}

TEST(BerlekampWelch, TestZero) {
	const int total = 40;
	const int required = 20;
	const int num_zeros = 200;
	const int num_nonzeros = 20;

	BerlekampWelchTest test(required, total);

	std::vector<uint8_t> buf(num_zeros);
	for (int i = 0; i < num_nonzeros; ++i) {
		buf.push_back(1);
	}

	auto [shares, callback] = test.StoreShares();
	test.Encode(buf, callback);

	(*shares)[0][0]++;

	test.DecodeTo(*shares, BerlekampWelchTest::noop);
}

TEST(BerlekampWelch, TestErrors) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;
	const int repetitions = 500;

	BerlekampWelchTest test(required, total);
	auto shares = test.SomeShares(block).second;
	test.DecodeTo(shares, BerlekampWelchTest::noop);

	for (int i = 0; i < repetitions; i++) {
		std::vector<std::pair<int, std::vector<uint8_t>>> shares_copy(shares.begin(), shares.end());
		for (int j = 0; j < block; j++) {
			test.MutateShare(j, shares_copy[test.randn(total)]);
			test.MutateShare(j, shares_copy[test.randn(total)]);
		}

		auto [decoded_shares, callback] = test.StoreShares();
		test.DecodeTo(shares, callback);

		auto shares_upto_k = shares;
		for (int a = required; a < total; ++a) {
			shares_upto_k.erase(a);
		}
		test.AssertEqualShares(shares_upto_k, *decoded_shares);
	}
}

TEST(BerlekampWelch, RandomShares) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;
	const int repetitions = 500;

	BerlekampWelchTest test(required, total);
	auto shares_map = test.SomeShares(block).second;
	test.DecodeTo(shares_map, BerlekampWelchTest::noop);
	std::vector<std::pair<int, std::vector<uint8_t>>> shares(shares_map.begin(), shares_map.end());

	for (int i = 0; i < repetitions; i++) {
		std::vector<std::pair<int, std::vector<uint8_t>>> test_shares = shares;
		test.PermuteShares(test_shares);
		test_shares.resize(required+2+test.randn(total-required-2));

		for (int i = 0; i < block; i++) {
			test.MutateShare(i, test_shares[test.randn(static_cast<int>(test_shares.size()))]);
		}

		auto [decoded_shares, callback] = test.StoreShares();
		test.DecodeTo(test_shares, callback);

		std::vector<std::pair<int, std::vector<uint8_t>>> firstNShares(shares.begin(), shares.begin() + required);;
		test.AssertEqualShares(firstNShares, *decoded_shares);
	}
}

// NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

} // namespace test
} // namespace infectious
