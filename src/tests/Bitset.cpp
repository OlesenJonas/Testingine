#include <bitset>
#include <cassert>
#include <intern/Datastructures/DynamicBitset.hpp>
#include <iostream>
#include <vcruntime.h>

int main()
{
    {
        DynamicBitset a{65};
        assert(a.getInternal().size() == 3);
        assert(a.getSize() == 65);
        assert(a.anyBitSet() == false);
        a.setBit(64);
        assert(a.anyBitSet() == true);
        a.resize(64);
        assert(a.anyBitSet() == false);
    }

    {
        DynamicBitset a{65};
        a.setBit(64);
        assert(a.getFirstBitSet() == 64);
        a.setBit(23);
        assert(a.getFirstBitSet() == 23);
        a.setBit(33);
        assert(a.getFirstBitSet() == 23);
        a.clearBit(23);
        assert(a.getFirstBitSet() == 33);
        a.toggleBit(23);
        assert(a.getFirstBitSet() == 23);
    }

    {
        DynamicBitset c{0};
        assert(c.getInternal().empty());
        assert(c.getSize() == 0);
        assert(c.anyBitSet() == false);
    }

    {
        int c = 33;
        int a = 8;
        int b = 27;
        DynamicBitset bitset{static_cast<uint32_t>(c)};
        assert(bitset.getSize() == c);
        assert(bitset.anyBitSet() == false);

        bitset.fill(a, b);
        assert(bitset.anyBitSet());
        for(int i = 0; i < a; i++)
        {
            assert(!bitset.getBit(i));
        }
        for(int i = a; i <= b; i++)
        {
            assert(bitset.getBit(i));
        }
        for(int i = b + 1; i < c; i++)
        {
            assert(!bitset.getBit(i));
        }
    }

    {
        int c = 133;
        int a = 32;
        int b = 95;
        DynamicBitset bitset{static_cast<uint32_t>(c)};
        assert(bitset.getSize() == c);
        assert(bitset.anyBitSet() == false);

        bitset.fill(a, b);
        assert(bitset.anyBitSet());
        for(int i = 0; i < a; i++)
        {
            assert(!bitset.getBit(i));
        }
        for(int i = a; i <= b; i++)
        {
            assert(bitset.getBit(i));
        }
        for(int i = b + 1; i < c; i++)
        {
            assert(!bitset.getBit(i));
        }
    }

    {
        int c = 35;
        int a = 4;
        int b = 31;
        DynamicBitset bitset{static_cast<uint32_t>(c)};
        assert(bitset.getSize() == c);
        assert(bitset.anyBitSet() == false);

        bitset.fill(a, b);
        assert(bitset.anyBitSet());
        for(int i = 0; i < a; i++)
        {
            assert(!bitset.getBit(i));
        }
        for(int i = a; i <= b; i++)
        {
            assert(bitset.getBit(i));
        }
        for(int i = b + 1; i < c; i++)
        {
            assert(!bitset.getBit(i));
        }
    }

    {
        int c = 77;
        int a = 32;
        int b = 63;
        DynamicBitset bitset{static_cast<uint32_t>(c)};
        assert(bitset.getSize() == c);
        assert(bitset.anyBitSet() == false);

        bitset.fill(a, b);
        assert(bitset.anyBitSet());
        for(int i = 0; i < a; i++)
        {
            assert(!bitset.getBit(i));
        }
        for(int i = a; i <= b; i++)
        {
            assert(bitset.getBit(i));
        }
        for(int i = b + 1; i < c; i++)
        {
            assert(!bitset.getBit(i));
        }
    }

    {
        int c = 35;
        int a = 18;
        int b = 18;
        DynamicBitset bitset{static_cast<uint32_t>(c)};
        assert(bitset.getSize() == c);
        assert(bitset.anyBitSet() == false);

        bitset.fill(a, b);
        assert(bitset.anyBitSet());
        for(int i = 0; i < a; i++)
        {
            assert(!bitset.getBit(i));
        }
        for(int i = a; i <= b; i++)
        {
            assert(bitset.getBit(i));
        }
        for(int i = b + 1; i < c; i++)
        {
            assert(!bitset.getBit(i));
        }
    }
}