#include <cinttypes>
#include <random>
#include <tuple>
#include <utility>
#include <vector>
#include "gtest/gtest.h"

#include "fec.hpp"

namespace infectious {

static std::random_device rd;
static std::mt19937 generator(rd());

struct BerlekampWelchTest : public FEC {
public:
	using FEC::FEC;

	std::pair<std::shared_ptr<std::vector<Share>>, ShareOutputFunc> StoreShares() const {
		auto out = std::make_shared<std::vector<Share>>(Total());

		return std::make_pair(out, [out](int num, Slice<uint8_t> data) {
			(*out)[num].num = num;
			(*out)[num].data = data.copy();
		});
	}

	std::pair<Slice<uint8_t>, std::vector<Share>> SomeShares(int block) const {
		// seed the initial data
		Slice<uint8_t> data(Required() * block);

		for (int i = 0; i < data.size(); ++i) {
			data[i] = static_cast<uint8_t>(i+1);
		}

		auto [shares, store] = StoreShares();
		Encode(data, store);

		return std::make_pair(data, *shares);
	}

	Slice<uint8_t> BerlekampWelch(std::vector<Share>& shares, int index) {
		return berlekampWelch(shares, index);
	}

	std::vector<Share> CopyShares(const std::vector<Share>& shares) {
		std::vector<Share> out(Total());
		for (unsigned int i = 0; i < shares.size(); ++i) {
			out[i].num = shares[i].num;
			out[i].data = shares[i].data.copy();
		}
		return out;
	}

	void MutateShare(int idx, Share share) {
		auto orig = share.data[idx];
		auto next = static_cast<uint8_t>(randn(256));
		while (next == orig) {
			next = static_cast<uint8_t>(randn(256));
		}
		share.data[idx] = next;
	}

	void PermuteShares(std::vector<Share>& shares) {
		for (unsigned int i = 0; i < shares.size(); i++) {
			auto with = randn(shares.size()-i) + i;
			std::swap(shares[i], shares[with]);
		}
	}

	void AssertEqualShares(int required, const std::vector<Share>& expected, const std::vector<Share>& got) {
		for (int i = 0; i < required; i++) {
			ASSERT_EQ(expected[i].num, got[i].num) << "shares[" << i << "].num does not match";
			ASSERT_EQ(expected[i].data, got[i].data) << "shares[" << i << "].data does not match";
		}
	}

	// can be used as a do-nothing output callback
	static void noop(int, Slice<uint8_t>) {}

	int randn(int limit) {
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

	auto out = test.BerlekampWelch(shares, 0);
	ASSERT_EQ(out, Slice<uint8_t>({0x01, 0x02, 0x03, 0x15, 0x69, 0xcc, 0xf2}));
}

TEST(BerlekampWelch, MultipleBlock) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto shares = test.SomeShares(block).second;

	test.DecodeTo(shares, BerlekampWelchTest::noop);

	shares[0].data[0]++;
	shares[1].data[0]++;

	auto [decoded_shares, callback] = test.StoreShares();
	test.DecodeTo(shares, callback);

	test.AssertEqualShares(required, shares, *decoded_shares);
}

TEST(BerlekampWelch, TestDecode) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto [origdata, shares] = test.SomeShares(block);

	Slice<uint8_t> output(origdata.size() + 1);
	int output_len = test.Decode(output, shares);
	output = output.slice(0, output_len);
	ASSERT_EQ(origdata, output);
}

TEST(BerlekampWelch, TestZero) {
	const int total = 40;
	const int required = 20;

	BerlekampWelchTest test(required, total);

	Slice<uint8_t> buf(200);
	for (int i = 0; i < 20; ++i) {
		buf.push_back(0x14);
	}

	auto [shares, callback] = test.StoreShares();
	test.Encode(buf, callback);

	(*shares)[0].data[0]++;

	test.DecodeTo(*shares, BerlekampWelchTest::noop);
}

TEST(BerlekampWelch, TestErrors) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto shares = test.SomeShares(block).second;
	test.DecodeTo(shares, BerlekampWelchTest::noop);

	for (int i = 0; i < 500; i++) {
		auto shares_copy = test.CopyShares(shares);
		for (int j = 0; j < block; j++) {
			test.MutateShare(j, shares_copy[test.randn(total)]);
			test.MutateShare(j, shares_copy[test.randn(total)]);
		}

		auto [decoded_shares, callback] = test.StoreShares();
		test.DecodeTo(shares, callback);

		test.AssertEqualShares(required, shares, *decoded_shares);
	}
}

TEST(BerlekampWelch, RandomShares) {
	const int block = 4096;
	const int total = 7;
	const int required = 3;

	BerlekampWelchTest test(required, total);
	auto shares = test.SomeShares(block).second;
	test.DecodeTo(shares, BerlekampWelchTest::noop);

	for (int i = 0; i < 500; i++) {
		auto test_shares = test.CopyShares(shares);
		test.PermuteShares(test_shares);
		test_shares.resize(required+2+test.randn(total-required-2));

		for (int i = 0; i < block; i++) {
			test.MutateShare(i, test_shares[test.randn(test_shares.size())]);
		}

		auto [decoded_shares, callback] = test.StoreShares();
		test.DecodeTo(test_shares, callback);

		test.AssertEqualShares(required, shares, *decoded_shares);
	}
}

} // namespace infectious
