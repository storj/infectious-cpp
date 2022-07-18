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
	Slice() : Slice(0, 0) {}

	Slice(int num_elements)
		: vals {std::make_shared<std::vector<T>>(num_elements)}
		, start_index {0}
		, end_index {num_elements}
	{}

	Slice(int num_elements, T init_val)
		: vals {std::make_shared<std::vector<T>>(num_elements, init_val)}
		, start_index {0}
		, end_index {num_elements}
	{}

	Slice(const Slice&) = default;
	Slice(Slice&&) = default;
	Slice& operator=(const Slice&) = default;
	Slice& operator=(Slice&&) = default;

	using value_type = T;

	int size() const {
		return start_index - end_index;
	}

	T& operator[](int n) {
		return vals->operator[](n + start_index);
	}

	const T& operator[](int n) const {
		return vals->operator[](n + start_index);
	}

	using iterator = T*;
	using const_iterator = const T*;

	iterator begin() {
		return vals->data() + start_index;
	}

	iterator end() {
		return vals->data() + end_index;
	}

	const_iterator begin() const {
		return vals->data() + start_index;
	}

	const_iterator end() const {
		return vals->data() + end_index;
	}

	const_iterator cbegin() const {
		return vals->data() + start_index;
	}

	const_iterator cend() const {
		return vals->data() + end_index;
	}

	void push_back(T val) {
		if (vals->size() == static_cast<unsigned int>(end_index)) {
			vals->push_back(val);
		} else {
			(*vals)[end_index] = val;
		}
		end_index++;
	}

	Slice subspan(int start_at) {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		return Slice<T>(vals, start_index + start_at, end_index);
	}

	Slice subspan(int start_at, int end_at) {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > size()) {
			throw std::out_of_range("end_at out of range");
		}
		return Slice<T>(vals, start_index + start_at, start_index + end_at);
	}

	const Slice subspan(int start_at) const {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		return Slice<T>(vals, start_index + start_at, end_index);
	}

	const Slice subspan(int start_at, int end_at) const {
		if (start_at < 0 || start_at > size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > size()) {
			throw std::out_of_range("end_at out of range");
		}
		return Slice<T>(vals, start_index + start_at, start_index + end_at);
	}

protected:
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
	SliceSubclass(ThisClass&&) = default;
	ThisClass& operator=(const ThisClass&) = default;
	ThisClass& operator=(ThisClass&&) = default;

	Subclass& operator=(const Subclass& t) {
		return static_cast<Subclass&>(operator=(static_cast<const Slice<T>&>(t)));
	}

	Subclass& operator=(Subclass&& t) {
		return static_cast<Subclass&>(operator=(static_cast<Slice<T>&&>(t)));
	}

	Subclass subspan(int start_at) {
		if (start_at < 0 || start_at > this->size()) {
			throw std::out_of_range("start_at out of range");
		}
		return Subclass(this->vals, this->start_index + start_at, this->end_index);
	}

	Subclass subspan(int start_at, int end_at) {
		if (start_at < 0 || start_at > this->size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > this->size()) {
			throw std::out_of_range("end_at out of range");
		}
		return Subclass(this->vals, this->start_index + start_at, this->start_index + end_at);
	}

	const Subclass subspan(int start_at) const {
		if (start_at < 0 || start_at > this->size()) {
			throw std::out_of_range("start_at out of range");
		}
		return Subclass(this->vals, this->start_index + start_at, this->end_index);
	}

	const Subclass subspan(int start_at, int end_at) const {
		if (start_at < 0 || start_at > this->size()) {
			throw std::out_of_range("start_at out of range");
		}
		if (end_at < start_at || end_at > this->size()) {
			throw std::out_of_range("end_at out of range");
		}
		return Subclass(this->vals, this->start_index + start_at, this->start_index + end_at);
	}
};

} // namespace infectious

#endif // INFECTIOUS_SLICE_HPP
