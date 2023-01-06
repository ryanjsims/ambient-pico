#pragma once

#include <memory>
#include <optional>
#include <span>

template <class T>
class circular_buffer {
public:
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

private:
	std::unique_ptr<T[]> buf_;
	size_t head_ = 0;
	size_t tail_ = 0;
	const size_t max_size_;
};