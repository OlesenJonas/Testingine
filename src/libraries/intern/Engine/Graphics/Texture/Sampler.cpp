#include "Sampler.hpp"

bool Sampler::Info::operator==(const Sampler::Info& rhs) const
{
#define EQUAL_MEMBER(member) member == rhs.member
    return EQUAL_MEMBER(magFilter) &&        //
           EQUAL_MEMBER(minFilter) &&        //
           EQUAL_MEMBER(mipMapFilter) &&     //
           EQUAL_MEMBER(addressModeU) &&     //
           EQUAL_MEMBER(addressModeV) &&     //
           EQUAL_MEMBER(addressModeW) &&     //
           EQUAL_MEMBER(enableAnisotropy) && //
           EQUAL_MEMBER(minLod) &&           //
           EQUAL_MEMBER(maxLod);
#undef EQUAL_MEMBER
}