#pragma once

#include "../Buffer/Buffer.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <intern/Datastructures/Pool.hpp>
#include <intern/Datastructures/Span.hpp>
#include <string>
#include <vector>
#include <vulkan/vulkan_core.h>

// todo: just contain two/three Handle<Buffer> for index + position/(position+attributes) !!

struct VertexInputDescription
{
    std::vector<VkVertexInputBindingDescription> bindings;
    std::vector<VkVertexInputAttributeDescription> attributes;
    static VertexInputDescription getDefault();
};

struct Mesh
{
    struct VertexAttributes
    {
        glm::vec3 normal;
        glm::vec3 color;
        glm::vec2 uv;
    };

    // std::vector<Vertex> vertices;

    // todo: does this need to be here?
    //  I feel like the actual GPU sided stuff, ie vertexcount, buffer handles etc should be seperate from the
    //  scene management realted stuff like name, parent, children etc
    std::string name{};

    uint32_t vertexCount = 0;

    Handle<Buffer> positionBuffer;
    Handle<Buffer> attributeBuffer;

    // bool loadFromObj(const char* filename);
};