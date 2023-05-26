#include "Scene.hpp"
#include "DefaultComponents.hpp"
#include "glTF/glTF.hpp"
#include "glTF/glTFtoVK.hpp"

#include <Datastructures/Pool.hpp>
#include <Engine.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>

ECS::Entity parseNode(
    Span<Handle<Mesh>> meshes,
    Span<Handle<MaterialInstance>> matInsts,
    const glTF::Main& gltf,
    ECS& ecs,
    uint32_t nodeIndex,
    ECS::Entity& parent)
{
    const glTF::Node& glTFNode = gltf.nodes[nodeIndex];

    auto nodeEntity = ecs.createEntity();
    auto* nodeTransform = nodeEntity.addComponent<Transform>();
    nodeTransform->position = glTFNode.translation;
    nodeTransform->orientation = glm::quat{
        glTFNode.rotationAsVec.w, glTFNode.rotationAsVec.x, glTFNode.rotationAsVec.y, glTFNode.rotationAsVec.z};
    nodeTransform->scale = glTFNode.scale;
    nodeTransform->calculateLocalTransformMatrix();

    auto* nodeHierarchy = nodeEntity.addComponent<Hierarchy>();
    nodeHierarchy->parent = parent.getID();

    if(glTFNode.meshIndex.has_value())
    {
        const glTF::Mesh& gltfMesh = gltf.meshes[glTFNode.meshIndex.value()];
        auto* renderInfo = nodeEntity.addComponent<RenderInfo>();
        // only primitive 0 gets loaded atm
        {
            renderInfo->mesh = meshes[glTFNode.meshIndex.value()];
            renderInfo->materialInstance = matInsts[gltfMesh.primitives[0].materialIndex];
        }
    }

    if(glTFNode.childNodeIndices.has_value())
    {
        for(uint32_t childNodeIndex : glTFNode.childNodeIndices.value())
        {
            ECS::Entity childEnTT = parseNode(meshes, matInsts, gltf, ecs, childNodeIndex, nodeEntity);
            nodeHierarchy->children.push_back(childEnTT.getID());
        }
    }

    return nodeEntity;
};

void Scene::load(std::string path)
{
    /*
        todo:
            confirm its actually a gltf file
    */

    std::filesystem::path basePath{path};
    basePath = basePath.parent_path();

    const glTF::Main gltf = glTF::Main::load(path);
    assert(gltf.asset.version == "2.0");

    ResourceManager* rm = Engine::get()->getResourceManager();

    // create samplers
    std::vector<Handle<Sampler>> samplers;
    samplers.resize(gltf.samplers.size());
    for(int i = 0; i < gltf.samplers.size(); i++)
    {
        auto& samplerInfo = gltf.samplers[i];
        samplers[i] = rm->createSampler(Sampler::Info{
            .magFilter = glTF::toVk::magFilter(samplerInfo.magFilter),
            .minFilter = glTF::toVk::minFilter(samplerInfo.minFilter),
            .mipmapMode = glTF::toVk::mipmapMode(samplerInfo.minFilter),
            .addressModeU = glTF::toVk::addressMode(samplerInfo.wrapS),
            .addressModeV = glTF::toVk::addressMode(samplerInfo.wrapT),
        });
    }

    // Load & create textures ("images" in glTF)
    std::vector<Handle<Texture>> textures;
    textures.resize(gltf.images.size());
    for(int i = 0; i < gltf.images.size(); i++)
    {
        std::filesystem::path imagePath = basePath / gltf.images[i].uri;
        textures[i] = rm->createTexture(imagePath.generic_string().c_str(), VK_IMAGE_USAGE_SAMPLED_BIT);
    }

    // Load buffers (CPU)
    std::vector<std::vector<char>> buffers;
    buffers.resize(gltf.buffers.size());
    for(int i = 0; i < gltf.buffers.size(); i++)
    {
        const auto& bufferInfo = gltf.buffers[i];
        std::filesystem::path bufferPath = basePath / bufferInfo.uri;

        std::ifstream file(bufferPath.c_str(), std::ios::ate | std::ios::binary);
        if(!file.is_open())
        {
            assert(false); // TODO: error handling
        }
        std::vector<char>& buffer = buffers[i];
        auto fileSize = (size_t)file.tellg();
        assert(fileSize == bufferInfo.byteLength);
        buffer.resize(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        file.seekg(0);
    }

    // Load/create Meshes
    std::vector<Handle<Mesh>> meshes;
    meshes.resize(gltf.meshes.size());
    for(int i = 0; i < gltf.meshes.size(); i++)
    {
        // not just using the bufferView/Accessors as defined by gltf
        // instead transforming the data so everything fits a unified mesh layout

        const glTF::Mesh& mesh = gltf.meshes[i];

        if(mesh.primitives.size() > 1)
        {
            BREAKPOINT;
            // for each primitive
            //      todo: handle more than just one
        }

        const auto& primitive = mesh.primitives[0];
        const glTF::Accessor& positionAccessor = gltf.accessors[primitive.attributes.positionAccessor];
        const glTF::Accessor& normalAccessor = gltf.accessors[primitive.attributes.normalAccessor];
        const glTF::Accessor& uv0Accessor = gltf.accessors[primitive.attributes.uv0Accessor];
        // todo: load vertex colors (if available)
        const uint32_t vertexCount = positionAccessor.count;
        assert(normalAccessor.count == vertexCount);
        assert(uv0Accessor.count == vertexCount);

        std::vector<Mesh::VertexAttributes> vertexAttributes;
        vertexAttributes.resize(vertexCount);
        std::vector<glm::vec3> vertexPositions;
        vertexPositions.resize(vertexCount);

        // read positions
        {
            const glTF::BufferView& bufferView = gltf.bufferViews[positionAccessor.bufferViewIndex];
            char* startAddress =
                &(buffers[bufferView.bufferIndex][bufferView.byteOffset + positionAccessor.byteOffset]);

            // Position must be of type float3
            assert(positionAccessor.componentType == glTF::Accessor::f32);
            assert(positionAccessor.type == glTF::Accessor::vec3);
            // for mesh attributes this should be != 0
            //       todo: if some files *do* have stride == 0, need to lookup stride for tight packing from
            //       size(componentType)*size(type)
            assert(bufferView.byteStride != 0);

            for(int j = 0; j < vertexCount; j++)
            {
                vertexPositions[j] =
                    *((glm::vec3*)(startAddress + static_cast<size_t>(j * bufferView.byteStride)));
            }
        }

        // read normals
        {
            const glTF::Accessor& accessor = normalAccessor;
            const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
            char* startAddress = &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

            // normals must be of type float3
            assert(accessor.componentType == glTF::Accessor::f32);
            assert(accessor.type == glTF::Accessor::vec3);
            // for mesh attributes this should be != 0
            //       todo: if some files *do* have stride == 0, need to lookup stride for tight packing from
            //       size(componentType)*size(type)
            assert(bufferView.byteStride != 0);

            for(int j = 0; j < vertexCount; j++)
            {
                vertexAttributes[j].normal =
                    *((glm::vec3*)(startAddress + static_cast<size_t>(j * bufferView.byteStride)));
            }
        }

        // read uvs
        {
            const glTF::Accessor& accessor = uv0Accessor;
            const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
            char* startAddress = &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

            // uvs must be of type float2
            assert(accessor.componentType == glTF::Accessor::f32);
            assert(accessor.type == glTF::Accessor::vec2);
            // for mesh attributes this should be != 0
            //       todo: if some files *do* have stride == 0, need to lookup stride for tight packing from
            //       size(componentType)*size(type)
            assert(bufferView.byteStride != 0);

            for(int j = 0; j < vertexCount; j++)
            {
                vertexAttributes[j].uv =
                    *((glm::vec2*)(startAddress + static_cast<size_t>(j * bufferView.byteStride)));
            }
        }

        std::vector<uint32_t> indices;
        if(primitive.indexAccessor.has_value())
        {
            const glTF::Accessor& accessor = gltf.accessors[primitive.indexAccessor.value()];
            indices.resize(accessor.count);
            const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
            char* startAddress = &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

            // indices must be scalar
            assert(accessor.type == glTF::Accessor::scalar);
            // for mesh attributes this should be != 0
            //       todo: if some files *do* have stride == 0, need to lookup stride for tight packing from
            //       size(componentType)*size(type)

            uint32_t byteStride = bufferView.byteStride;
            assert(byteStride == 0 && "TODO: if not 0");

            if(accessor.componentType == glTF::Accessor::ComponentType::uint16)
            {
                Span<uint16_t> data{(uint16_t*)startAddress, accessor.count};
                for(int j = 0; j < accessor.count; j++)
                {
                    indices[j] = data[j];
                }
            }
            else if(accessor.componentType == glTF::Accessor::ComponentType::uint32)
            {
                Span<uint32_t> data{(uint32_t*)startAddress, accessor.count};
                for(int j = 0; j < accessor.count; j++)
                {
                    indices[j] = data[j];
                }
            }
            else
            {
                assert(false && "TODO? does this even occur?");
            }
        }
        else
        {
            indices.resize(vertexAttributes.size());
            for(int j = 0; j < indices.size(); j++)
                indices[j] = j;
        }

        meshes[i] = rm->createMesh(vertexPositions, vertexAttributes, indices, mesh.name);
    }

    // Create material instances (materials in glTF)
    std::vector<Handle<MaterialInstance>> materialInstances;
    materialInstances.resize(gltf.materials.size());
    auto basicPBRMaterial = rm->getMaterial("PBRBasic");
    assert(rm->get(basicPBRMaterial) != nullptr);
    for(int i = 0; i < gltf.materials.size(); i++)
    {
        const glTF::Material& material = gltf.materials[i];
        const glTF::Texture& normalTextureGLTF = gltf.textures[material.normalTexture.index];

        const Handle<Texture> normalTexture = textures[normalTextureGLTF.sourceIndex];
        const Handle<Sampler> normalSampler = samplers[normalTextureGLTF.samplerIndex];

        auto materialInstanceHandle = rm->createMaterialInstance(basicPBRMaterial);
        MaterialInstance* matInst = rm->get(materialInstanceHandle);
        matInst->parameters.setResource("normalTexture", rm->get(normalTexture)->sampledResourceIndex);
        matInst->parameters.setResource("normalSampler", rm->get(normalSampler)->resourceIndex);
        matInst->parameters.pushChanges();

        materialInstances[i] = materialInstanceHandle;
    }

    ECS& ecs = Engine::get()->ecs;

    // Load scene(s)
    //  just the first one for now, not sure how to handle multiple
    //  and if im even planning on using files with multiple
    {
        for(uint32_t rootIndex : gltf.scenes[0].nodeIndices)
        {
            parseNode(meshes, materialInstances, gltf, ecs, rootIndex, Engine::get()->sceneRoot);
        }
    }

    // UPDATE TRANSFORM HIERARCHY !!!

    BREAKPOINT;
}