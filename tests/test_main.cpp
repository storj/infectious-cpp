// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#include "gtest/gtest.h"

#include "random_env.hpp"

namespace infectious::test {

// make assertion failures throw
class ThrowListener : public ::testing::EmptyTestEventListener {
	void OnTestPartResult(const ::testing::TestPartResult& result) override {
		if (result.type() == ::testing::TestPartResult::kFatalFailure) {
			throw ::testing::AssertionException(result);
		}
	}
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
RandomEnvironment* random_env;

} // namespace infectious::test

// GTest's functions here are taking ownership but don't use
// smart pointers or gsl::owner<>.
// NOLINTBEGIN(cppcoreguidelines-owning-memory)

auto main(int argc, char** argv) -> int {
	::infectious::test::random_env = new ::infectious::test::RandomEnvironment;
	::testing::AddGlobalTestEnvironment(::infectious::test::random_env);

	::testing::InitGoogleTest(&argc, argv);
	::testing::UnitTest::GetInstance()->listeners().Append(new ::infectious::test::ThrowListener);
	return RUN_ALL_TESTS();
}

// NOLINTEND(cppcoreguidelines-owning-memory)
