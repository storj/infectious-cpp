// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_RANDOM_ENV_HPP
#define INFECTIOUS_RANDOM_ENV_HPP

#include <random>
#include "gtest/gtest.h"

namespace infectious::test {

struct RandomEnvironment : public ::testing::Environment {
public:
	RandomEnvironment()
		: generator(rd())
	{}

	~RandomEnvironment() override = default;

	RandomEnvironment(const RandomEnvironment&) = delete;
	RandomEnvironment(RandomEnvironment&&) = delete;
	auto operator=(const RandomEnvironment&) -> RandomEnvironment& = delete;
	auto operator=(RandomEnvironment&&) -> RandomEnvironment& = delete;

	[[nodiscard]] auto randn(int limit) -> int {
		std::uniform_int_distribution<> distrib(0, limit - 1);
		return distrib(generator);
	}

	std::random_device rd;
	std::mt19937 generator;
};

// There's no good way to access a ::testing::Environment from tests
// other than through a global (or a variable with static lifetime,
// which isn't much better).
// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern RandomEnvironment* random_env;

} // namespace infectious::test

#endif // ifndef INFECTIOUS_RANDOM_ENV_HPP
