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

template <typename T>
class Slice {
protected:
	Slice(std::shared_ptr<std::vector<T>> vals_, int start_index_, int end_index_)
		: vals {std::move(vals_)}
		, start_index {start_index_}
		, end_index {end_index_}
	{}

public:
	Slice()
		: vals {std::make_shared<std::vector<T>>(0)}
		, start_index {0}
		, end_index {0}
	{}

	explicit Slice(int num_elements)
		: vals {std::make_shared<std::vector<T>>(num_elements)}
		, start_index {0}
		, end_index {num_elements}
	{}

	Slice(int num_elements, T init_val)
		: vals {std::make_shared<std::vector<T>>(num_elements, init_val)}
		, start_index {0}
		, end_index {num_elements}
	{}

	explicit Slice(std::vector<T> init)
		: vals {std::make_shared<std::vector<T>>(std::move(init))}
		, start_index {0}
		, end_index {static_cast<int>(vals->size())}
	{}

	template <std::input_iterator IterType>
	Slice(IterType&& ibegin, IterType&& iend)
		: vals {std::make_shared<std::vector<T>>(std::forward<IterType>(ibegin), std::forward<IterType>(iend))}
		, start_index {0}
		, end_index {static_cast<int>(iend - ibegin)}
	{}

	Slice(const Slice<T>&) = default;
	Slice(Slice<T>&&) noexcept = default;
	~Slice() = default;
	auto operator=(const Slice<T>&) -> Slice<T>& = default;
	auto operator=(Slice<T>&&) noexcept -> Slice<T>& = default;

	using value_type = T;

	[[nodiscard]] auto size() const -> int {
		return end_index - start_index;
	}

	[[nodiscard]] auto operator[](int n) -> T& {
		return vals->operator[](n + start_index);
	}

	[[nodiscard]] auto operator[](int n) const -> const T& {
		return vals->operator[](n + start_index);
	}

	using iterator = T*;
	using const_iterator = const T*;

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

	void push_back(T val) {
		if (vals->size() == static_cast<unsigned int>(end_index)) {
			vals->push_back(val);
		} else {
			(*vals)[end_index] = val;
		}
		end_index++;
	}

	template <typename Range>
	void extend(const Range& r) {
		vals->insert(vals->end(), r.begin(), r.end());
	}

	[[nodiscard]] auto slice(int start_at) const -> Slice {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		return Slice<T>(vals, start_index + start_at, end_index);
	}

	[[nodiscard]] auto slice(int start_at, int end_at) const -> Slice {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > size()) {
			throw std::out_of_range("end_at out of range");
		}
		return Slice<T>(vals, start_index + start_at, start_index + end_at);
	}

	[[nodiscard]] auto operator==(const Slice<T>& other) const -> bool {
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

	[[nodiscard]] auto operator<=>(const Slice<T>& other) const -> int {
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

	[[nodiscard]] auto copy() const -> Slice<T> {
		auto new_vec = std::make_shared<std::vector<T>>(begin(), end());
		return Slice(std::move(new_vec), 0, size());
	}

	[[nodiscard]] auto backing_vector() const -> std::shared_ptr<std::vector<T>> {
		return vals;
	}

	[[nodiscard]] auto get_start_index() const -> int {
		return start_index;
	}

	[[nodiscard]] auto get_end_index() const -> int {
		return end_index;
	}

private:
	std::shared_ptr<std::vector<T>> vals;
	int start_index;
	int end_index;
};

template <typename T, typename Subclass>
class SliceSubclass : public Slice<T> {
public:
	using Slice<T>::Slice;
	using ThisClass = SliceSubclass<T, Subclass>;

	SliceSubclass(const ThisClass&) = default;
	SliceSubclass(ThisClass&&) noexcept = default;
	~SliceSubclass() = default;
	auto operator=(const ThisClass&) -> ThisClass& = default;
	auto operator=(ThisClass&&) noexcept -> ThisClass& = default;

	// This is kind of a weird use of the CRTP; not surprised clang-tidy doesn't like it.
	// NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
	auto operator=(const Subclass& t) -> Subclass& {
		return static_cast<Subclass&>(operator=(static_cast<const Slice<T>&>(t)));
	}

	// This is kind of a weird use of the CRTP; not surprised clang-tidy doesn't like it.
	// NOLINTNEXTLINE(cppcoreguidelines-c-copy-assignment-signature,misc-unconventional-assign-operator)
	auto operator=(Subclass&& t) -> Subclass& {
		return static_cast<Subclass&>(operator=(static_cast<Slice<T>&&>(t)));
	}

	[[nodiscard]] auto slice(int start_at) const -> Subclass {
		if (start_at < 0 || start_at > this->size()) {
			throw std::out_of_range("start_at out of range");
		}
		return Subclass(this->backing_vector(), this->get_start_index() + start_at, this->get_end_index());
	}

	[[nodiscard]] auto slice(int start_at, int end_at) const -> Subclass {
		if (start_at < 0 || start_at > this->size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > this->size()) {
			throw std::out_of_range("end_at out of range");
		}
		return Subclass(this->backing_vector(), this->get_start_index() + start_at, this->get_start_index() + end_at);
	}
};

} // namespace infectious

#endif // INFECTIOUS_SLICE_HPP
