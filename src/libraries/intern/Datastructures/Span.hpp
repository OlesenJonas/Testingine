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
    using element_type = T;
    using value_type = std::remove_cv_t<T>;
    using ptr_type = T*;
    using ref_type = T&;
    using iterator = T*;
    using const_iterator = const T*;

    Span() : _data(nullptr), _length(0){};

    Span(ptr_type ptr, size_type size) : _data(ptr), _length(size){};

    Span(ptr_type first, ptr_type last) : _data(first), _length(last - first + 1){};

    // construct const Span from non-const span, not sure how robust. Test more when needed
    // template <
    //     typename V, typename U = T, std::enable_if_t<std::is_const_v<U>, bool> = true,
    //     std::enable_if_t<!std::is_const_v<V>, bool> = true>
    // explicit Span(const Span<V>& s) : _data(s._data), _length(s._length){};

    template <typename U = T, std::enable_if_t<std::is_const_v<U>, bool> = true>
    Span(std::initializer_list<T> l) : _data(l.begin()), _length(l.size()){};

    Span(std::vector<T>& c) : _data(c.data()), _length(c.size()){};

    template <typename U = std::remove_const<T>>
    Span(const std::vector<U>& c) : _data(c.data()), _length(c.size()){};

    // inline?
    ref_type operator[](size_type i) const
    {
        assert(_data != nullptr && i < _length && "Accesing element outside of span");
        return _data[i]; // NOLINT
    }

    [[nodiscard]] size_type size() const
    {
        return _length;
    }

    [[nodiscard]] bool empty() const
    {
        return _length == 0;
    }

    ptr_type data()
    {
        return _data;
    }

    ptr_type constData() const
    {
        return _data;
    }

    bool operator==(const Span& rhs)
    {
        return _data == rhs._data && _length == rhs._length;
    }

    iterator begin()
    {
        return &_data[0];
    }
    const_iterator begin() const
    {
        return &_data[0];
    }
    iterator end()
    {
        return &_data[_length];
    }
    const_iterator end() const
    {
        return &_data[_length];
    }

  private:
    ptr_type _data = nullptr;
    size_type _length = size_type(0);
};

// Container -> Span conversions
// template stuff waay easier this way
//     template <typename Container>
//     auto makeSpan(Container& v)
//         -> decltype(Span(std::declval<Container&>().data(), std::declval<Container&>().size()))
// {
//     return Span(v.data(), v.size());
// }

template <typename C>
using GetElementType = std::remove_reference_t<decltype(std::declval<C&>()[0])>;

template <typename Container>
auto makeSpan(Container& v) -> Span<GetElementType<Container>>
{
    return Span(v.data(), v.size());
}
template <typename Container>
auto makeConstSpan(const Container& v) -> Span<const GetElementType<Container>>
{
    return Span(v.data(), v.size());
}

// for everything else just construct from ptr+size