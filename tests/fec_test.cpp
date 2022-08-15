#include <algorithm>
#include <array>
#include <map>
#include <random>
#include <utility>
#include <vector>
#include "gtest/gtest.h"

#include "infectious/fec.hpp"

namespace infectious {
namespace test {

// for some reason, clang-tidy doesn't much care for gtest's macros.
// NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

extern std::random_device rd;

std::vector<int> perm(int n) {
	std::vector<int> nums(n);
	std::iota(nums.begin(), nums.end(), 0);
	std::shuffle(nums.begin(), nums.end(), rd);
	return nums;
}

TEST(FEC, BasicOperation) {
	const int block = 1024 * 1024;
	const int total = 40;
	const int required = 20;

	FEC code(required, total);

	// seed the initial data
	std::vector<uint8_t> data(required*block);
	for (int i = 0; i < static_cast<int>(data.size()); ++i) {
		data[i] = static_cast<uint8_t>(i);
	}

	// encode it and store to outputs
	std::map<int, std::vector<uint8_t>> outputs;
	auto store = [&](int num, std::basic_string_view<uint8_t> output_data) {
		outputs[num] = std::vector(output_data.begin(), output_data.end());
	};
	code.Encode(data, store);

	// pick required of the total shares randomly
	std::vector<std::pair<int, std::vector<uint8_t>>> shares(required);
	auto share_nums = perm(total);
	for (int i = 0; i < required; ++i) {
		shares[i].first = share_nums[i];
		shares[i].second = outputs[share_nums[i]];
	}

	std::vector<uint8_t> got(required*block);
	auto record = [&](int num, std::basic_string_view<uint8_t> output_data) {
		std::copy(output_data.begin(), output_data.end(), &got[num*block]);
	};
	code.Rebuild(shares, record);

	ASSERT_EQ(data, got) << "reconstructed data did not match";
}

TEST(FEC, EncodeSingle) {
	const int block = 1024 * 1024;
	const int total = 40;
	const int required = 20;

	FEC code(required, total);

	// seed the initial data
	std::vector<uint8_t> data(required*block);
	for (int i = 0; i < static_cast<int>(data.size()); ++i) {
		data[i] = static_cast<uint8_t>(i);
	}

	// encode it and store to outputs
	std::map<int, std::vector<uint8_t>> outputs;
	for (int i = 0; i < total; i++) {
		outputs[i] = std::vector<uint8_t>(block);
		code.EncodeSingle(i, data.begin(), data.end(), outputs[i].begin(), outputs[i].end());
	}

	// pick required of the total shares randomly
	std::vector<std::pair<int, std::vector<uint8_t>>> shares(required);
	auto share_nums = perm(total);
	for (int i = 0; i < required; ++i) {
		shares[i].first = share_nums[i];
		shares[i].second = outputs[share_nums[i]];
	}

	std::vector<uint8_t> got(required*block);
	auto record = [&](int num, std::basic_string_view<uint8_t> output_data) {
		std::copy(output_data.begin(), output_data.end(), &got[num*block]);
	};
	code.Rebuild(shares, record);

	ASSERT_EQ(data, got) << "reconstructed data did not match";
}

// NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

} // namespace test
} // namespace infectious
