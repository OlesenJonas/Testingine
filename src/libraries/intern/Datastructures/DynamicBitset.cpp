#include <bit>
#include <cassert>

#include "Datastructures/DynamicBitset.hpp"
#include "DynamicBitset.hpp"

DynamicBitset::DynamicBitset(uint32_t _size) : size(_size)
{
    internal.resize(UintDivAndCeil(size, 32));
    std::fill(internal.begin(), internal.end(), 0);
}

bool DynamicBitset::anyBitSet() const
{
    for(auto i = 0; i < internal.size(); i++)
    {
        if(internal[i] != 0u)
        {
            return true;
        }
    }
    return false;
}

bool DynamicBitset::anyBitClear() const
{
    for(auto i = 0; i < internal.size() - 1; i++)
    {
        if(internal[i] != 0xFFFFFFFF)
        {
            return true;
        }
    }
    uint32_t lastInt = internal[internal.size() - 1];
    const uint32_t ones = 0xFFFFFFFF;
    const uint32_t bitsInLastInt = size % 32u;
    const uint32_t mask = ~(ones >> (32u - bitsInLastInt));
    // set all bits in the last int beyond "size" to 1
    lastInt |= mask;
    // if the whole int is now 111....111 then there was also no 0 in the first part of the int
    return !(lastInt == ones);
}

void DynamicBitset::setBit(uint32_t index)
{
    assert(index < size);
    const uint32_t intIndex = index / 32u;
    internal[intIndex] |= (1u << (index % 32u));
}
void DynamicBitset::clearBit(uint32_t index)
{
    assert(index < size);
    const uint32_t intIndex = index / 32u;
    internal[intIndex] &= ~(1u << (index % 32u));
}
void DynamicBitset::toggleBit(uint32_t index)
{
    assert(index < size);
    const uint32_t intIndex = index / 32u;
    internal[intIndex] ^= (1u << (index % 32u));
}
bool DynamicBitset::getBit(uint32_t index) const
{
    assert(index < size);
    const uint32_t intIndex = index / 32u;
    return (internal[intIndex] & (1u << (index % 32u))) != 0u;
}

void DynamicBitset::clear()
{
    for(auto i = 0; i < internal.size(); i++)
        internal[i] = 0u;
}
void DynamicBitset::fill()
{
    for(auto i = 0; i < internal.size(); i++)
        internal[i] = ~0u;
    fixLastInteger();
}
void DynamicBitset::fill(uint32_t firstBit, uint32_t lastBit)
{
    assert(firstBit < size && lastBit < size);
    if(lastBit == firstBit)
    {
        setBit(firstBit);
        return;
    }
    assert(firstBit < lastBit);

    const uint32_t ones = 0xFFFFFFFF;

    // Fast path: all bits to fill are contained in the same int
    if(firstBit / 32u == lastBit / 32u)
    {
        const uint32_t amountOfBitsToFill = lastBit - firstBit + 1;
        uint32_t fillBits = ones >> (32u - amountOfBitsToFill);
        fillBits = fillBits << (firstBit % 32u);
        internal[firstBit / 32u] |= fillBits;
        return;
    }

    // fill integers completly contained in [firstBit, lastBit]
    const int32_t firstFullint = UintDivAndCeil(firstBit, 32u);
    const int32_t lastFullint = (lastBit + 1u) / 32u - 1u;

    for(int i = firstFullint; i <= lastFullint; i++)
    {
        internal[i] = 0xFFFFFFFF;
    }

    uint32_t fillBits = 0u;

    // partial int at start
    const uint32_t amountOfBitsInFirstInt = 32u - (firstBit % 32u);
    fillBits = ones >> (32u - amountOfBitsInFirstInt);
    fillBits = fillBits << (firstBit % 32u);
    internal[firstBit / 32u] |= fillBits;

    // partial int at end
    const uint32_t amountOfBitsInLastInt = (lastBit % 32u) + 1u;
    fillBits = ones >> (32u - amountOfBitsInLastInt);
    internal[lastBit / 32u] |= fillBits;
}

constexpr int32_t getLastFull(uint32_t i)
{
    return (i + 1u) / 32u - 1u;
}
static_assert(getLastFull(0) == -1);
static_assert(getLastFull(1) == -1);
static_assert(getLastFull(30) == -1);
static_assert(getLastFull(31) == 0);
static_assert(getLastFull(32) == 0);
static_assert(getLastFull(62) == 0);
static_assert(getLastFull(63) == 1);
static_assert(getLastFull(64) == 1);

uint32_t DynamicBitset::getFirstBitSet() const
{
    for(auto i = 0; i < internal.size() - 1; i++)
    {
        if(internal[i] != 0u)
        {
            return 32u * i + std::countr_zero(internal[i]);
        }
    }
    if(internal[internal.size() - 1] == 0)
    {
        return 0xFFFFFFFF;
    }
    return 32u * (internal.size() - 1) + std::countr_zero(internal[internal.size() - 1]);
}

uint32_t DynamicBitset::getFirstBitClear() const
{
    // todo: I feel like there must be simpler logic for this
    for(auto i = 0; i < internal.size() - 1; i++)
    {
        if(internal[i] != 0xFFFFFFFF)
        {
            return 32u * i + std::countr_one(internal[i]);
        }
    }
    uint32_t consecOnesInLastInt = std::countr_one(internal[internal.size() - 1]);
    if(size % 32u == 0u)
    {
        return internal[internal.size() - 1] == 0xFFFFFFFF ? 0xFFFFFFFF
                                                           : 32u * (internal.size() - 1) + consecOnesInLastInt;
    }
    return (consecOnesInLastInt == size % 32u) ? 0xFFFFFFFF : 32u * (internal.size() - 1) + consecOnesInLastInt;
}

uint32_t DynamicBitset::getSize() const
{
    return size;
}

void DynamicBitset::resize(uint32_t newSize)
{
    const uint32_t oldSize = size;
    size = newSize;
    const uint32_t oldInternalSize = internal.size();
    const uint32_t newInternalSize = UintDivAndCeil(newSize, 32u);

    if(newInternalSize > internal.size())
    {
        internal.resize(newInternalSize);
        // the "leftover" bits in the last integer are required to be 0
        // so only the newly added ints need to be cleared
        // std::fill ?
        for(auto i = oldInternalSize; i < newInternalSize; i++)
        {
            internal[i] = 0u;
        }
        return;
    }

    if(newInternalSize == internal.size())
    {
        if(newSize > oldSize)
        {
            return;
        }
        // else
        fixLastInteger();
        return;
    }

    if(newInternalSize < internal.size())
    {
        internal.resize(newInternalSize);
        fixLastInteger();
        return;
    }
}
const std::vector<uint32_t>& DynamicBitset::getInternal() const
{
    return internal;
}

void DynamicBitset::fixLastInteger()
{
    if((size % 32u) == 0)
        return;

    // need to ensure that all bits in internal past "size" are 0

    const uint32_t bitsToKeep = (size % 32u);
    const uint32_t allOnes = ~0u;
    const uint32_t keepMask = allOnes >> (32 - bitsToKeep);

    auto& lastInt = internal[internal.size() - 1];
    lastInt &= keepMask;

    assert(std::popcount(lastInt) <= bitsToKeep);
}