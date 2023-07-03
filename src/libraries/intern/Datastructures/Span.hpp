#pragma once

#include <array>
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <vector>

template <typename T>
class Span
{
  public:
    using size_type = std::size_t;
    using value_type = std::remove_const_t<T>;

    // Constructors ----------------------------

    Span() : _data(nullptr), _length(0){};

    Span(T* ptr, size_t size) : _data(ptr), _length(size){};

    Span(T* first, T* last) : _data(first), _length(last - first + 1){};

    Span(Span<T>& span) = default;
    Span(const Span<T>& span) = default;

    // for converting from Span<T> to Span<const T>
    Span(Span<value_type>& span)
        requires std::is_const_v<T>
        : _data(span.data()), _length(span.size()){};

    Span(std::initializer_list<value_type> list)
        requires std::is_const_v<T>
        : _data(list.begin()), _length(list.size()){};

    Span(const std::vector<value_type>& vec)
        requires std::is_const_v<T>
        : _data(vec.data()), _length(vec.size()){};

    Span(std::vector<T>& vec) : _data(vec.data()), _length(vec.size()){};

    template <std::size_t N>
    Span(std::array<value_type, N>& vec)
        requires std::is_const_v<T>
        : _data(vec.data()), _length(vec.size()){};

    template <std::size_t N>
    Span(std::array<T, N>& vec) : _data(vec.data()), _length(vec.size()){};

    // Member functions ----------------------------

    // inline?
    T& operator[](size_t index)
    {
        assert(_data != nullptr && index < _length && "Accesing element outside of span");
        return _data[index]; // NOLINT
    }

    const T& operator[](size_t index) const
    {
        assert(_data != nullptr && index < _length && "Accesing element outside of span");
        return _data[index]; // NOLINT
    }

    [[nodiscard]] size_t size() const
    {
        return _length;
    }

    [[nodiscard]] bool empty() const
    {
        return _length == 0;
    }

    T* data()
    {
        return _data;
    }

    T* data() const
    {
        return _data;
    }

    bool operator==(const Span& rhs)
    {
        return _data == rhs._data && _length == rhs._length;
    }

    T* begin()
    {
        return &_data[0];
    }
    const T* begin() const
    {
        return &_data[0];
    }
    T* end()
    {
        return &_data[_length];
    }
    const T* end() const
    {
        return &_data[_length];
    }

  private:
    T* _data = nullptr;
    size_t _length = size_type(0);
};