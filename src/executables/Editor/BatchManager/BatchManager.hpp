#pragma once

#include <Engine/Graphics/Material/Material.hpp>
#include <Engine/Graphics/Mesh/Mesh.hpp>

#include <unordered_map>

/*
    Does not need to batch by material instance, since that can be changed dynamically in shader!!
*/
class BatchManager
{
  public:
    using BatchIndex = uint32_t;

    BatchManager() = default;

    inline BatchIndex getBatchCount() const { return freeIndex; }
    BatchIndex getBatchIndex(Material::Handle material, Mesh::Handle mesh);

    struct MatMeshPair
    {
        Material::Handle mat;
        Mesh::Handle mesh;

        static_assert(sizeof(Material::Handle) == sizeof(uint32_t));

        bool operator==(const MatMeshPair&) const = default;

        struct Hash
        {
            [[nodiscard]] inline uint64_t operator()(const MatMeshPair& pair) const
            {
                return ((uint64_t)pair.mat.hash() << 32u) + pair.mesh.hash();
            }
        };
    };
    using BatchLUT_t = std::unordered_map<MatMeshPair, BatchIndex, MatMeshPair::Hash>;
    const BatchLUT_t& getLUT();

  private:
    BatchLUT_t LUT;
    BatchIndex freeIndex = (BatchIndex)0;
};