#include "Material.hpp"
#include <intern/ResourceManager/ResourceManager.hpp>

Handle<Material> ResourceManager::createMaterial(Material mat, std::string_view name)
{
    // todo: handle naming collisions
    auto iterator = nameToMaterialLUT.find(name);
    assert(iterator == nameToMaterialLUT.end());

    Handle<Material> newMaterialHandle = materialPool.insert(mat);

    nameToMaterialLUT.insert({std::string{name}, newMaterialHandle});

    return newMaterialHandle;
}