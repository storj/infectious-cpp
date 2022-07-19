// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_COMMON_HPP
#define INFECTIOUS_COMMON_HPP

#include <stdexcept>

namespace infectious {

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

} // namespace infectious

#endif // INFECTIOUS_COMMON_HPP
