#pragma once

#include <cstdint>
#include <vector>

// todo: for bigger sizes looping over all internal ints can become problematic,
//       idea: template an amount of levels N, where each level is another DynamicBitset, with each bit
//       indicating if one of X integers in the level below has a bit set
//       eg: A 2nd level could be a single uint32_t where each bit indicates if the corresponding full uint at
//       level 0 has any bit set so testing for firstBitSet, anyBitSet in 1024 bits would require just checking the
//       2nd level int first to see which integer contains the bit and then just a single check inside that
//       integer, instead of looping over 1024/32 ints
class DynamicBitset
{
  public:
    explicit DynamicBitset(uint32_t _size);

    friend DynamicBitset operator&(const DynamicBitset& lhs, const DynamicBitset& rhs);

    void setBit(uint32_t index);
    void clearBit(uint32_t index);
    void toggleBit(uint32_t index);
    [[nodiscard]] bool getBit(uint32_t index) const;

    void clear();
    void clear(uint32_t firstBit, uint32_t lastBit);
    void fill();
    void fill(uint32_t firstBit, uint32_t lastBit);

    bool anyBitClear() const;
    bool anyBitSet() const;
    // returns 0xffffffff is no bit is set
    [[nodiscard]] uint32_t getFirstBitSet() const;
    // returns 0xffffffff is no bit is cleared
    [[nodiscard]] uint32_t getFirstBitClear() const;
    // returns 0xffffffff is none found
    [[nodiscard]] uint32_t getNextBitSet(uint32_t index) const;

    [[nodiscard]] uint32_t getSize() const;
    void resize(uint32_t newSize);

    [[nodiscard]] const std::vector<uint32_t>& getInternal() const;

  private:
    static inline uint32_t UintDivAndCeil(uint32_t x, uint32_t y) // NOLINT
    {
        return (x + y - 1) / y;
    }

    uint32_t size = 0;
    std::vector<uint32_t> internal;
    /*
        Some functions require that any bits in internal with position
        >= size are 0 for optimization purposes.
        This needs to be ensured!
    */
    void fixLastInteger();
};