#include "BatchManager.hpp"

BatchManager::BatchIndex BatchManager::getBatchIndex(Material::Handle material, Mesh::Handle mesh)
{
    auto iter = LUT.find({material, mesh});
    if(iter != LUT.end())
    {
        return iter->second;
    }

    BatchIndex newIndex = freeIndex++;
    LUT[{material, mesh}] = newIndex;

    return newIndex;
}

const BatchManager::BatchLUT_t& BatchManager::getLUT() { return LUT; }