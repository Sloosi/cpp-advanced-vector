#pragma once

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <new>
#include <memory>
#include <tuple>
#include <utility>

template <typename T>
class RawMemory {
public:
    RawMemory() = default;

    explicit RawMemory(size_t capacity) :
        buffer_(Allocate(capacity)),
        capacity_(capacity)
    {}

    RawMemory(RawMemory&& other) noexcept : 
        buffer_(std::move(other.buffer_)),
        capacity_(std::move(other.capacity_))
    {}

    RawMemory<T>& operator=(RawMemory&& rhs) noexcept {
        buffer_ = std::move(rhs.buffer_);
        capacity_ = std::move(rhs.capacity_);

        return *this;
    }

    ~RawMemory() {
        Deallocate(buffer_);
    }

    T* operator+(size_t offset) noexcept {
        assert(offset <= capacity_);
        return buffer_ + offset;
    }

    const T* operator+(size_t offset) const noexcept {
        return const_cast<RawMemory&>(*this) + offset;
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<RawMemory&>(*this)[index];
    }

    T& operator[](size_t index) noexcept {
        assert(index < capacity_);
        return buffer_[index];
    }

    void Swap(RawMemory& other) noexcept {
        std::swap(buffer_, other.buffer_);
        std::swap(capacity_, other.capacity_);
    }

    const T* GetAddress() const noexcept {
        return buffer_;
    }

    T* GetAddress() noexcept {
        return buffer_;
    }

    size_t Capacity() const {
        return capacity_;
    }

private:
    T* Allocate(size_t n) {
        return n != 0 ? static_cast<T*>(operator new(n * sizeof(T))) : nullptr;
    }

    void Deallocate(T* buf) noexcept {
        operator delete(buf);
    }

    T* buffer_ = nullptr;
    size_t capacity_ = 0;
};

template <typename T>
class Vector {
public:
    using iterator = T*;
    using const_iterator = const T*;

    Vector() = default;

    explicit Vector(size_t size) :
        data_(size),
        size_(size)
    {
        std::uninitialized_value_construct_n(data_.GetAddress(), size);
    }

    Vector(const Vector& other) :
        data_(other.size_),
        size_(other.size_)
    {
        std::uninitialized_copy_n(other.data_.GetAddress(), other.size_, data_.GetAddress());
    }

    Vector(Vector&& other) noexcept {
        Swap(other);
    }

    ~Vector() {
        std::destroy_n(data_.GetAddress(), size_);
    }

    Vector& operator=(const Vector& rhs) {
        if (this != &rhs) {
            if (rhs.size_ > data_.Capacity()) {
                Vector<T> rhs_copy(rhs);
                Swap(rhs_copy);
            }
            else {
                if (size_ > rhs.size_) {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + rhs.size_, data_.GetAddress());
                    std::destroy_n(data_.GetAddress() + rhs.size_, size_ - rhs.size_);
                }
                else {
                    std::copy(rhs.data_.GetAddress(), rhs.data_.GetAddress() + size_, data_.GetAddress());
                    std::uninitialized_copy_n(rhs.data_.GetAddress() + size_, rhs.size_ - size_, data_.GetAddress() + size_);
                }
                size_ = rhs.size_;
            }
        }
        return *this;
    }

    Vector& operator=(Vector&& rhs) noexcept {
        if (this != &rhs) {
            Swap(rhs);
        }
        return *this;
    }

    iterator begin() noexcept {
        return data_.GetAddress();
    }

    iterator end() noexcept {
        return data_.GetAddress() + size_;
    }

    const_iterator begin() const noexcept {
        return const_iterator(data_.GetAddress());
    }

    const_iterator end() const noexcept {
        return const_iterator(data_.GetAddress() + size_);
    }

    const_iterator cbegin() const noexcept {
        return begin();
    }

    const_iterator cend() const noexcept {
        return end();
    }

    void Reserve(size_t new_capacity) {
        if (new_capacity <= data_.Capacity()) {
            return;
        }

        RawMemory<T> new_data(new_capacity);
        UninitializedConstructN(data_.GetAddress(), size_, new_data.GetAddress());
        std::destroy_n(data_.GetAddress(), size_);
        data_.Swap(new_data);
    }

    void Resize(size_t new_size) {
        if (new_size < size_) {
            std::destroy_n(data_.GetAddress() + new_size, size_ - new_size);
            size_ = new_size;
        }
        else {
            Reserve(new_size);
            std::uninitialized_value_construct_n(data_.GetAddress() + size_, new_size - size_);
            size_ = new_size;
        }
    }

    void Swap(Vector& other) noexcept {
        data_.Swap(other.data_);
        std::swap(size_, other.size_);
    }

    size_t Size() const noexcept {
        return size_;
    }

    size_t Capacity() const noexcept {
        return data_.Capacity();
    }

    template <typename... Args>
    T& EmplaceBack(Args&&... args) {
        T* result = nullptr;
        if (size_ == Capacity()) {
            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);
            result = new (new_data + size_) T(std::forward<Args>(args)...);
            UninitializedConstructN(data_.GetAddress(), size_, new_data.GetAddress());
            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);
        }
        else {
            result = new (data_ + size_) T(std::forward<Args>(args)...);
        }
        ++size_;
        return *result;
    }

    void PopBack() noexcept {
        if (size_ > 0) {
            std::destroy_at(data_.GetAddress() + size_ - 1);
            --size_;
        }
    }

    void PushBack(const T& value) {
        EmplaceBack(std::forward<const T&>(value));
    }

    void PushBack(T&& value) {
        EmplaceBack(std::forward<T&&>(value));
    }

    template <typename... Args>
    iterator Emplace(const_iterator pos, Args&&... args) {
        assert(pos >= begin() && pos <= end());
        int offset = pos - begin();

        if (data_.Capacity() <= size_) {

            RawMemory<T> new_data(size_ == 0 ? 1 : size_ * 2);

            new (new_data.GetAddress() + offset) T(std::forward<Args>(args)...);

            UninitializedConstructN(data_.GetAddress(), offset, new_data.GetAddress());
            UninitializedConstructN(data_.GetAddress() + offset, size_ - offset, new_data.GetAddress() + offset + 1);

            std::destroy_n(data_.GetAddress(), size_);
            data_.Swap(new_data);

        }
        else {

            try {

                if (pos != end()) {

                    T temp(std::forward<Args>(args)...);
                    new (end()) T(std::forward<T>(*(end() - 1)));

                    std::move_backward(begin() + offset, end() - 1, end());
                    *(begin() + offset) = std::forward<T>(temp);

                }
                else {
                    new (end()) T(std::forward<Args>(args)...);
                }

            }
            catch (...) {
                operator delete (end());
                throw;
            }
        }

        ++size_;
        return begin() + offset;
    }

    iterator Erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable_v<T>) {
        assert(pos >= begin() && pos < end());
        size_t offset = pos - begin();
        std::move(begin() + offset + 1, end(), begin() + offset);
        PopBack();
        return begin() + offset;
    }

    iterator Insert(const_iterator pos, const T& value) {
        return Emplace(pos, value);
    }

    iterator Insert(const_iterator pos, T&& value) {
        return Emplace(pos, std::move(value));
    }

    T& operator[](size_t index) noexcept {
        assert(index < size_);
        return data_[index];
    }

    const T& operator[](size_t index) const noexcept {
        return const_cast<Vector&>(*this)[index];
    }

private:
    void UninitializedConstructN(T* data, int pos, T* new_data) {
        if constexpr (std::is_nothrow_move_constructible_v<T> || !std::is_copy_constructible_v<T>) {
            std::uninitialized_move_n(data, pos, new_data);
        }
        else {
            std::uninitialized_copy_n(data, pos, new_data);
        }
    }

    RawMemory<T> data_;
    size_t size_ = 0;
};