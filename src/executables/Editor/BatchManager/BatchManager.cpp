#include "BatchManager.hpp"

BatchManager::BatchManager() { batches.reserve(100); }

bool BatchManager::tupleLess(const MatMeshTuple& left, const MatMeshTuple& right)
{
    if(left._0 != right._0)
        return left._0.getIndex() < right._0.getIndex();
    return left._1.getIndex() < right._1.getIndex();
}

void BatchManager::createBatch(Material::Handle material, Mesh::Handle mesh)
{
    const MatMeshTuple asTuple{material, mesh};
    const auto iter = std::lower_bound(batches.begin(), batches.end(), asTuple, BatchManager::tupleLess);

    if(iter != batches.end() && *iter == asTuple)
        return; // batch already exists

    batches.insert(iter, asTuple);
}

BatchManager::BatchIndex BatchManager::getBatchIndex(Material::Handle mat, Mesh::Handle mesh) const
{
    const auto iter =
        std::lower_bound(batches.begin(), batches.end(), MatMeshTuple{mat, mesh}, BatchManager::tupleLess);
    assert(iter != batches.end());
    return std::distance(batches.begin(), iter);
}

const BatchManager::BatchList& BatchManager::getBatchList() const { return batches; }