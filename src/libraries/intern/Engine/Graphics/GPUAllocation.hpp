#pragma once

#include "Buffer/Buffer.hpp"

struct GPUAllocation
{
    Buffer::Handle buffer;
    size_t offset = 0;
    size_t size = 0;
    void* ptr = nullptr;
};