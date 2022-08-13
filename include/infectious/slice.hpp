// Copyright (C) 2022 Storj Labs, Inc.
// See LICENSE for copying information.

#ifndef INFECTIOUS_SLICE_HPP
#define INFECTIOUS_SLICE_HPP

#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace infectious {

template <typename PtrType>
class byteslice {
protected:
	byteslice(PtrType&& vals_, int start_index_, int end_index_)
		: vals {std::forward<PtrType>(vals_)}
		, start_index {start_index_}
		, end_index {end_index_}
	{
		if (end_index < start_index) {
			throw std::out_of_range("end_index < start_index");
		}
	}

public:
	using pointer_type = PtrType;

	byteslice(const byteslice<PtrType>&) = default;
	byteslice(byteslice<PtrType>&&) noexcept = default;
	~byteslice() = default;
	auto operator=(const byteslice<PtrType>&) -> byteslice<PtrType>& = default;
	auto operator=(byteslice<PtrType>&&) noexcept -> byteslice<PtrType>& = default;

	[[nodiscard]] auto size() const -> int {
		return end_index - start_index;
	}

	[[nodiscard]] auto operator[](int n) -> uint8_t& {
		if (n < 0 || n >= size()) {
			throw std::out_of_range("access out of range");
		}
		return vals->operator[](n + start_index);
	}

	[[nodiscard]] auto operator[](int n) const -> const uint8_t& {
		if (n < 0 || n >= size()) {
			throw std::out_of_range("access out of range");
		}
		return vals->operator[](n + start_index);
	}

	using iterator = uint8_t*;
	using const_iterator = const uint8_t*;

	[[nodiscard]] auto begin() -> iterator {
		return &(vals->data())[start_index];
	}

	[[nodiscard]] auto end() -> iterator {
		return &(vals->data())[end_index];
	}

	[[nodiscard]] auto begin() const -> const_iterator {
		return &(vals->data())[start_index];
	}

	[[nodiscard]] auto end() const -> const_iterator {
		return &(vals->data())[end_index];
	}

	[[nodiscard]] auto cbegin() const -> const_iterator {
		return &(vals->data())[start_index];
	}

	[[nodiscard]] auto cend() const -> const_iterator {
		return &(vals->data())[end_index];
	}

	void push_back(uint8_t val) {
		if (vals->size() == static_cast<unsigned int>(end_index)) {
			vals->push_back(val);
		} else {
			(*vals)[end_index] = val;
		}
		end_index++;
	}

	[[nodiscard]] auto slice(int start_at) const -> byteslice<PtrType> {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		return byteslice<PtrType>(vals, start_index + start_at, end_index);
	}

	[[nodiscard]] auto slice(int start_at, int end_at) const -> byteslice<PtrType> {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > size()) {
			throw std::out_of_range("end_at out of range");
		}
		return byteslice<PtrType>(vals, start_index + start_at, start_index + end_at);
	}

	[[nodiscard]] auto operator==(const byteslice<PtrType>& other) const -> bool {
		if (size() != other.size()) {
			return false;
		}
		for (int i = 0; i < size(); ++i) {
			if (operator[](i) != other[i]) {
				return false;
			}
		}
		return true;
	}

	[[nodiscard]] auto operator<=>(const byteslice<PtrType>& other) const -> int {
		for (int i = 0; i < size(); ++i) {
			if (i >= other.size()) {
				return 1;
			}
			if (operator[](i) < other[i]) {
				return -1;
			}
			if (operator[](i) > other[i]) {
				return 1;
			}
		}
		if (other.size() > size()) {
			return -1;
		}
		return 0;
	}

	[[nodiscard]] auto copy() const -> byteslice<PtrType> {
		auto new_vec = std::make_shared<typename PtrType::element_type>(begin(), end());
		return byteslice<PtrType>(std::move(new_vec), 0, size());
	}

	[[nodiscard]] auto backing_vector() const -> PtrType {
		return vals;
	}

	[[nodiscard]] auto get_start_index() const -> int {
		return start_index;
	}

	[[nodiscard]] auto get_end_index() const -> int {
		return end_index;
	}

private:
	PtrType vals;
	int start_index;
	int end_index;
};

using SafeByteSlice = byteslice<std::shared_ptr<std::vector<uint8_t>>>;
using BareByteSlice = byteslice<uint8_t*>;

} // namespace infectious

#endif // INFECTIOUS_SLICE_HPP
