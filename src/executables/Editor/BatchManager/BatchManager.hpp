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
    using MatMeshTuple = CTuple<Material::Handle, Mesh::Handle>;
    using BatchList = std::vector<MatMeshTuple>;

    // returns ​true if the first argument is less than (i.e. is ordered before) the second
    static bool tupleLess(const MatMeshTuple& left, const MatMeshTuple& right);

    BatchManager();

    inline BatchIndex getBatchCount() const { return batches.size(); }
    void createBatch(Material::Handle material, Mesh::Handle mesh);
    BatchIndex getBatchIndex(Material::Handle mat, Mesh::Handle mesh) const;

  private:
    BatchList batches;

  public:
    const BatchList& getBatchList() const;
};