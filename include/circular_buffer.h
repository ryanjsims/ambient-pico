#pragma once

#include <memory>
#include <optional>
#include <span>

#include <iterator>

template <class T>
class circular_buffer {
public:
	struct iterator {
		//using iterator_concept = std::input_iterator;
		using iterator_category = std::input_iterator_tag;
		using difference_type = std::ptrdiff_t;
		using value_type = T;
		using pointer = T*;
		using reference = T&;

		iterator() = delete;
		iterator(pointer ptr, size_t index, size_t max_size);

		reference operator*() const;
		pointer operator->();

		iterator& operator++();
		iterator operator++(int);

		iterator operator+(int rhs);

		bool operator==(const iterator& rhs);
		bool operator!=(const iterator& rhs);

	private:
		pointer ptr_;
		size_t index_, max_size_;
	};
	explicit circular_buffer(size_t size) :
		buf_(std::unique_ptr<T[]>(new T[size])),
		max_size_(size)
	{ 
        // empty
    }

	bool put(T item);
    size_t put(std::span<T> items);
	std::optional<T> get();
    size_t get(std::span<T> &items);
	void reset();
	bool empty() const;
	bool full() const;
	size_t capacity() const;
	size_t size() const;

	void advance(size_t amount);

	iterator begin() const;
	iterator end() const;

private:
	std::unique_ptr<T[]> buf_;
	size_t head_ = 0;
	size_t tail_ = 0;
	const size_t max_size_;
};