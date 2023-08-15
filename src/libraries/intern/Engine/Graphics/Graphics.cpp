#include "Graphics.hpp"

bool ResourceStateMulti::containsUniformBufferUsage()
{
    return ((*this) & ResourceState::UniformBuffer)            //
           || ((*this) & ResourceState::UniformBufferGraphics) //
           || ((*this) & ResourceState::UniformBufferCompute);
}

bool ResourceStateMulti::containsStorageBufferUsage()
{
    return ((*this) & ResourceState::Storage)           //
           || ((*this) & ResourceState::StorageCompute) //
           || ((*this) & ResourceState::StorageGraphics);
}