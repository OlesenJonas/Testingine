#pragma once

#include <cstdint>
#include <vector>

class DynamicBitset
{
  public:
    explicit DynamicBitset(uint32_t _size);

    void setBit(uint32_t index);
    void clearBit(uint32_t index);
    void toggleBit(uint32_t index);
    [[nodiscard]] bool getBit(uint32_t index) const;

    void clear();
    // void clear(uint32_t firstBit, uint32_t lastBit);
    void fill();
    void fill(uint32_t firstBit, uint32_t lastBit);

    bool anyBitSet() const;
    // returns 0xffffffff is no bit was set
    [[nodiscard]] uint32_t getFirstBitSet() const;

    [[nodiscard]] uint32_t getSize() const;
    void resize(uint32_t newSize);

    [[nodiscard]] const std::vector<uint32_t>& getInternal() const;

  private:
    static inline uint32_t UintDivAndCeil(uint32_t x, uint32_t y) // NOLINT
    {
        return (x + y - 1) / y;
    }

    void fixLastInteger();

    uint32_t size = 0;
    /*
        Some functions require that any bits in internal with position
        >= size are 0 for optimization purposes.
        This needs to be ensured!
    */
    std::vector<uint32_t> internal;
};