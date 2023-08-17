#pragma once

#include <cstdint>
#include <type_traits>

// Handle and Pool types as shown in https://twitter.com/SebAaltonen/status/1562747716584648704 and realted tweets
template <typename T>
class Handle
{
  public:
    Handle() = default;
    Handle(uint32_t index, uint32_t generation) : index(index), generation(generation){};
    static Handle Invalid() { return {0, 0}; }
    static Handle Null() { return Invalid(); }
    [[nodiscard]] bool isValid() const { return generation != 0u; }
    bool operator==(const Handle<T>& other) const
    {
        return index == other.index && generation == other.generation;
    }
    bool operator!=(const Handle<T>& other) const
    {
        return index != other.index || generation != other.generation;
    }
    [[nodiscard]] uint32_t hash() const { return (uint32_t(index) << 16u) + uint32_t(generation); }
    [[nodiscard]] auto getIndex() const { return index; }
    [[nodiscard]] auto getGeneration() const { return generation; }

  private:
    uint16_t index = 0;
    uint16_t generation = 0;
};

static_assert(std::is_trivially_copyable_v<Handle<Handle<int>>>);