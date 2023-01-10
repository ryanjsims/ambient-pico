#include "circular_buffer.h"

template <class T>
bool circular_buffer<T>::put(T item) {
    if(!full()) {
        buf_[head_] = item;
        head_ = (head_ + 1) % max_size_;
        return true;
    }
    return false;
}

template <class T>
size_t circular_buffer<T>::put(std::span<T> items) {
    size_t count = 0;
    for(count; count < items.size() && put(items[count]); count++);
    return count;
}

template <class T>
std::optional<T> circular_buffer<T>::get() {
    if(empty()) {
        return std::nullopt;
    }
    T val = buf_[tail_];
    tail_ = (tail_ + 1) % max_size_;
    return val;
}

template <class T>
size_t circular_buffer<T>::get(std::span<T> &items) {
    size_t count = 0;
    for(count; count < items.size(); count++) {
        std::optional<T> val = get();
        if(!val) {
            break;
        }
        items[count] = *val;
    }
    return count;
}

template <class T>
void circular_buffer<T>::reset() {
    head_ = tail_;
}

template <class T>
bool circular_buffer<T>::empty() const {
    return head_ == tail_;
}

template <class T>
bool circular_buffer<T>::full() const {
    return ((head_ + 1) % max_size_) == tail_;
}

template <class T>
size_t circular_buffer<T>::capacity() const {
    return max_size_;
}

template <class T>
size_t circular_buffer<T>::size() const {
    size_t size = max_size_;

	if(!full())
	{
		if(head_ >= tail_)
		{
			size = head_ - tail_;
		}
		else
		{
			size = max_size_ + head_ - tail_;
		}
	}

	return size;
}

template <class T>
void circular_buffer<T>::advance(size_t amount) {
    tail_ = (tail_ + amount) % max_size_;
}

template <class T>
circular_buffer<T>::iterator circular_buffer<T>::begin() const {
    return iterator(buf_.get(), tail_, max_size_);
}

template <class T>
circular_buffer<T>::iterator circular_buffer<T>::end() const {
    return iterator(buf_.get(), head_ + 1, max_size_);
}

template <class T>
circular_buffer<T>::iterator::iterator(pointer ptr, size_t index, size_t max_size): ptr_(ptr), index_(index), max_size_(max_size) {}

template <class T>
circular_buffer<T>::iterator::reference circular_buffer<T>::iterator::operator*() const {
    return ptr_[index_ % max_size_];
}

template <class T>
circular_buffer<T>::iterator::pointer circular_buffer<T>::iterator::operator->() {
    return &ptr_[index_ % max_size_];
}

template <class T>
circular_buffer<T>::iterator& circular_buffer<T>::iterator::operator++() {
    index_++;
    return *this;
}

template <class T>
circular_buffer<T>::iterator circular_buffer<T>::iterator::operator++(int) {
    iterator tmp = *this;
    index_++;
    return tmp;
}

template <class T>
circular_buffer<T>::iterator circular_buffer<T>::iterator::operator+(int rhs) {
    iterator tmp(ptr_, index_ + rhs, max_size_);
    return tmp;
}

template <class T>
bool circular_buffer<T>::iterator::operator==(const iterator& rhs) {
    return ((std::ptrdiff_t)(ptr_ + index_) % max_size_) == ((std::ptrdiff_t)(rhs.ptr_ + rhs.index_) % max_size_);
}

template <class T>
bool circular_buffer<T>::iterator::operator!=(const iterator& rhs) {
    return !(*this == rhs);
}

template class circular_buffer<uint8_t>;