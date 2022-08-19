// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include <algorithm>
#include <array>
#include <map>
#include <random>
#include <utility>
#include <vector>
#include "gtest/gtest.h"

#include "infectious/fec.hpp"
#include "random_env.hpp"

namespace infectious::test {

// for some reason, clang-tidy doesn't much care for gtest's macros.
// NOLINTBEGIN(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

std::vector<int> perm(int n) {
	std::vector<int> nums(n);
	std::iota(nums.begin(), nums.end(), 0);
	std::shuffle(nums.begin(), nums.end(), random_env->rd);
	return nums;
}

TEST(FEC, BasicOperation) {
	const size_t block = 1024UL * 1024UL;
	const size_t total = 40;
	const size_t required = 20;

	FEC code(required, total);

	// seed the initial data
	std::vector<uint8_t> data(required*block);
	for (size_t i = 0; i < data.size(); ++i) {
		data[i] = static_cast<uint8_t>(i);
	}

	// encode it and store to outputs
	std::map<int, std::vector<uint8_t>> outputs;
	auto store = [&](int num, std::basic_string_view<uint8_t> output_data) {
		outputs[num] = std::vector(output_data.begin(), output_data.end());
	};
	code.Encode(data, store);

	// pick required of the total shares randomly
	std::vector<std::pair<size_t, std::vector<uint8_t>>> shares(required);
	auto share_nums = perm(total);
	for (size_t i = 0; i < required; ++i) {
		shares[i].first = share_nums[i];
		shares[i].second = outputs[share_nums[i]];
	}

	std::vector<uint8_t> got(required*block);
	auto record = [&](int num, std::basic_string_view<uint8_t> output_data) {
		std::copy(output_data.begin(), output_data.end(), &got[static_cast<size_t>(num)*block]);
	};
	code.Rebuild(shares, record);

	ASSERT_EQ(data, got) << "reconstructed data did not match";
}

TEST(FEC, EncodeSingle) {
	const size_t block = 1024UL * 1024UL;
	const size_t total = 40;
	const size_t required = 20;

	FEC code(required, total);

	// seed the initial data
	std::vector<uint8_t> data(required*block);
	for (size_t i = 0; i < data.size(); ++i) {
		data[i] = static_cast<uint8_t>(i);
	}

	// encode it and store to outputs
	std::map<int, std::vector<uint8_t>> outputs;
	for (int i = 0; static_cast<size_t>(i) < total; i++) {
		outputs[i] = std::vector<uint8_t>(block);
		code.EncodeSingle(i, data.begin(), data.end(), outputs[i].begin(), outputs[i].end());
	}

	// pick required of the total shares randomly
	std::vector<std::pair<int, std::vector<uint8_t>>> shares(required);
	auto share_nums = perm(total);
	for (size_t i = 0; i < required; ++i) {
		shares[i].first = share_nums[i];
		shares[i].second = outputs[share_nums[i]];
	}

	std::vector<uint8_t> got(required*block);
	auto record = [&](int num, std::basic_string_view<uint8_t> output_data) {
		std::copy(output_data.begin(), output_data.end(), &got[static_cast<size_t>(num)*block]);
	};
	code.Rebuild(shares, record);

	ASSERT_EQ(data, got) << "reconstructed data did not match";
}

// NoCopyBytes is a class holding a byte string which should only be copyable
// using explicit calls to its begin() and end() methods.
class NoCopyBytes {
public:
	template <typename... Args>
	NoCopyBytes(Args&&... args)
		: s_ptr(new std::basic_string<uint8_t>(std::forward<Args>(args)...))
	{}

	NoCopyBytes(NoCopyBytes&& other) = default;
	auto operator=(NoCopyBytes&& other) -> NoCopyBytes& = default;
	~NoCopyBytes() = default;

	NoCopyBytes(const NoCopyBytes& other) = delete;
	auto operator=(const NoCopyBytes& other) -> NoCopyBytes& = delete;

	[[nodiscard]] auto begin() -> std::basic_string<uint8_t>::iterator {
		return s_ptr->begin();
	}

	[[nodiscard]] auto end() -> std::basic_string<uint8_t>::iterator {
		return s_ptr->end();
	}

	[[nodiscard]] auto begin() const -> std::basic_string<uint8_t>::const_iterator {
		return s_ptr->begin();
	}

	[[nodiscard]] auto end() const -> std::basic_string<uint8_t>::const_iterator {
		return s_ptr->end();
	}

	[[nodiscard]] size_t size() const {
		return s_ptr->size();
	}

	auto operator==(const NoCopyBytes& other) const -> bool {
		return *s_ptr == *other.s_ptr;
	}

private:
	std::unique_ptr<std::basic_string<uint8_t>> s_ptr;
};

// NoCopyMap is a class holding a mapping of integers to NoCopyBytes which
// should only be copyable using explicit calls to its begin() and end()
// methods.
class NoCopyMap {
public:
	NoCopyMap()
		: m_ptr(new std::map<int, NoCopyBytes>())
	{}

	NoCopyMap(NoCopyMap&& other) = default;
	auto operator=(NoCopyMap&& other) -> NoCopyMap& = default;
	~NoCopyMap() = default;

	NoCopyMap(const NoCopyMap& other) = delete;
	auto operator=(const NoCopyMap& other) -> NoCopyMap& = delete;

	[[nodiscard]] auto size() -> size_t {
		return m_ptr->size();
	}

	[[nodiscard]] auto begin() -> std::map<int, NoCopyBytes>::iterator {
		return m_ptr->begin();
	}

	[[nodiscard]] auto end() -> std::map<int, NoCopyBytes>::iterator {
		return m_ptr->end();
	}

	template <typename... Args>
	auto emplace(Args&&... args) {
		return m_ptr->emplace(std::forward<Args>(args)...);
	}

private:
	std::unique_ptr<std::map<int, NoCopyBytes>> m_ptr;
};

// Test that non-implicitly-copyable objects can be used with the FEC methods.
TEST(FEC, NoImplicitCopies) {
	const size_t total = 40;
	const size_t required = 20;

	NoCopyMap picky_map;
	NoCopyBytes input_data(reinterpret_cast<const uint8_t*>("<Wash> Hey, I've been in a firefight before!  Well, I was in a fire.  Actually, I was fired from a fry-cook opportunity."));

	FEC fec(required, total);
	fec.Encode(input_data, [&](int num, const ByteView& output) {
		// only keep some of the shares, so decoding has to do both copying and polynomial division
		if (static_cast<size_t>(num) >= (total - required) - (required/2)) {
			picky_map.emplace(num, output);
		}
	});

	NoCopyBytes decode_result(input_data.size(), '\0');
	fec.Decode(picky_map, decode_result);

	ASSERT_EQ(input_data, decode_result);
}

// NOLINTEND(cert-err58-cpp,cppcoreguidelines-avoid-non-const-global-variables,cppcoreguidelines-owning-memory,modernize-use-trailing-return-type)

} // namespace infectious::test
