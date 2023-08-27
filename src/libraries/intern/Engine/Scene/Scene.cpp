#include "Scene.hpp"
#include "DefaultComponents.hpp"
#include "glTF/glTF.hpp"
#include "glTF/glTFtoTI.hpp"

#include <Datastructures/Pool/Pool.hpp>
#include <Engine/Application/Application.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>

ECS::Entity parseNode(
    Span<Mesh::Handle> meshes,
    Span<MaterialInstance::Handle> matInsts,
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

void Scene::load(std::string path, ECS* ecs, ECS::Entity parent)
{
    /*
        todo:
            confirm its actually a gltf file
    */

    auto* rm = ResourceManager::impl();

    std::filesystem::path basePath{path};
    basePath = basePath.parent_path();

    const glTF::Main gltf = glTF::Main::load(path);
    assert(gltf.asset.version == "2.0");

    // create samplers
    std::vector<Handle<Sampler>> samplers;
    samplers.resize(gltf.samplers.size());
    for(int i = 0; i < gltf.samplers.size(); i++)
    {
        auto& samplerInfo = gltf.samplers[i];
        samplers[i] = rm->createSampler(Sampler::Info{
            .magFilter = glTF::toEngine::magFilter(samplerInfo.magFilter),
            .minFilter = glTF::toEngine::minFilter(samplerInfo.minFilter),
            .mipMapFilter = glTF::toEngine::mipmapMode(samplerInfo.minFilter),
            .addressModeU = glTF::toEngine::addressMode(samplerInfo.wrapS),
            .addressModeV = glTF::toEngine::addressMode(samplerInfo.wrapT),
        });
    }

    // Load & create textures ("images" in glTF)
    // first find out which textures arent linear (all those that are used as basecolor textures)
    std::vector<bool> textureIsLinear;
    textureIsLinear.resize(gltf.images.size());
    for(int i = 0; i < textureIsLinear.size(); i++)
    {
        textureIsLinear[i] = true;
    }
    for(int i = 0; i < gltf.materials.size(); i++)
    {
        const glTF::Material& material = gltf.materials[i];
        const glTF::Texture& baseColorTextureGLTF =
            gltf.textures[material.pbrMetallicRoughness.baseColorTexture.index];
        textureIsLinear[baseColorTextureGLTF.sourceIndex] = false;
    }

    std::vector<Texture::Handle> textures;
    textures.resize(gltf.images.size());
    for(int i = 0; i < gltf.images.size(); i++)
    {
        // todo: mark textures used for baseColor in an earlier step, then load only those as non-linear
        std::filesystem::path imagePath = basePath / gltf.images[i].uri;
        textures[i] = rm->createTexture(Texture::LoadInfo{
            .path = imagePath.generic_string(),
            .fileDataIsLinear = textureIsLinear[i],
            .mipLevels = Texture::MipLevels::All,
            .fillMipLevels = true,
            .allStates = ResourceState::SampleSource,
            .initialState = ResourceState::SampleSource,
        });
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
    std::vector<Mesh::Handle> meshes;
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
        std::vector<Mesh::PositionType> vertexPositions;
        vertexPositions.resize(vertexCount);

        // read positions
        {
            const glTF::BufferView& bufferView = gltf.bufferViews[positionAccessor.bufferViewIndex];
            char* startAddress =
                &(buffers[bufferView.bufferIndex][bufferView.byteOffset + positionAccessor.byteOffset]);

            // Position must be of type float3
            assert(positionAccessor.componentType == glTF::Accessor::f32);
            assert(positionAccessor.type == glTF::Accessor::vec3);
            auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 3;

            for(int j = 0; j < vertexCount; j++)
            {
                vertexPositions[j] = *((glm::vec3*)(startAddress + static_cast<size_t>(j * effectiveStride)));
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
            auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 3;

            for(int j = 0; j < vertexCount; j++)
            {
                vertexAttributes[j].normal =
                    *((glm::vec3*)(startAddress + static_cast<size_t>(j * effectiveStride)));
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
            auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 2;

            for(int j = 0; j < vertexCount; j++)
            {
                vertexAttributes[j].uv = *((glm::vec2*)(startAddress + static_cast<size_t>(j * effectiveStride)));
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

        // read or create tangents
        {
            const bool glTFhasTangents = primitive.attributes.tangentAccessor.has_value();
            if(glTFhasTangents)
            {
                const glTF::Accessor& accessor = gltf.accessors[primitive.attributes.tangentAccessor.value()];
                const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
                char* startAddress =
                    &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

                // tangents must be of type float4
                assert(accessor.componentType == glTF::Accessor::f32);
                assert(accessor.type == glTF::Accessor::vec4);
                auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 4;

                for(int j = 0; j < vertexCount; j++)
                {
                    vertexAttributes[j].tangent =
                        *((glm::vec4*)(startAddress + static_cast<size_t>(j * effectiveStride)));
                    vertexAttributes[j].tangent.w *= -1;
                }
            }
            else
            {
                Mesh::generateTangents(vertexPositions, vertexAttributes, indices);
            }
        }

        meshes[i] = rm->createMesh(vertexPositions, vertexAttributes, indices, mesh.name);
    }

    // Create material instances (materials in glTF)
    std::vector<MaterialInstance::Handle> materialInstances;
    materialInstances.resize(gltf.materials.size());
    auto basicPBRMaterial = rm->getMaterial("PBRBasic");
    assert(rm->get(basicPBRMaterial) != nullptr);
    for(int i = 0; i < gltf.materials.size(); i++)
    {
        const glTF::Material& material = gltf.materials[i];
        auto matInst = rm->createMaterialInstance(basicPBRMaterial);

        const glTF::Texture& normalTextureGLTF = gltf.textures[material.normalTexture.index];
        const Texture::Handle normalTextureHandle = textures[normalTextureGLTF.sourceIndex];
        const Handle<Sampler> normalSamplerHandle = samplers[normalTextureGLTF.samplerIndex];
        Sampler* normalSampler = rm->get(normalSamplerHandle);
        MaterialInstance::setResource(matInst, "normalTexture", *rm->get<ResourceIndex>(normalTextureHandle));
        MaterialInstance::setResource(matInst, "normalSampler", normalSampler->sampler.resourceIndex);

        const glTF::Texture& baseColorTextureGLTF =
            gltf.textures[material.pbrMetallicRoughness.baseColorTexture.index];
        const Texture::Handle baseColorTextureHandle = textures[baseColorTextureGLTF.sourceIndex];
        const Handle<Sampler> baseColorSamplerHandle = samplers[baseColorTextureGLTF.samplerIndex];
        Sampler* baseColorSampler = rm->get(baseColorSamplerHandle);
        MaterialInstance::setResource(
            matInst, "baseColorTexture", *rm->get<ResourceIndex>(baseColorTextureHandle));
        MaterialInstance::setResource(matInst, "baseColorSampler", baseColorSampler->sampler.resourceIndex);

        const glTF::Texture& metalRoughTextureGLTF =
            gltf.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.index];
        const Texture::Handle metalRoughTextureHandle = textures[metalRoughTextureGLTF.sourceIndex];
        const Handle<Sampler> metalRoughSamplerHandle = samplers[metalRoughTextureGLTF.samplerIndex];
        Sampler* metalRoughSampler = rm->get(metalRoughSamplerHandle);
        MaterialInstance::setResource(
            matInst, "metalRoughTexture", *rm->get<ResourceIndex>(metalRoughTextureHandle));
        MaterialInstance::setResource(matInst, "metalRoughSampler", metalRoughSampler->sampler.resourceIndex);

        const glTF::Texture& occlusionTextureGLTF = gltf.textures[material.occlusionTexture.index];
        const Texture::Handle occlusionTextureHandle = textures[occlusionTextureGLTF.sourceIndex];
        const Handle<Sampler> occlusionSamplerHandle = samplers[occlusionTextureGLTF.samplerIndex];
        Sampler* occlusionSampler = rm->get(occlusionSamplerHandle);
        MaterialInstance::setResource(
            matInst, "occlusionTexture", *rm->get<ResourceIndex>(occlusionTextureHandle));
        MaterialInstance::setResource(matInst, "occlusionSampler", occlusionSampler->sampler.resourceIndex);

        materialInstances[i] = matInst;
    }

    ECS::Entity sceneRoot = parent;
    // Load scene(s)
    //  just the first one for now, not sure how to handle multiple
    //  (and if im even planning on using files with multiple)
    {
        for(uint32_t rootIndex : gltf.scenes[0].nodeIndices)
        {
            ECS::Entity rootNode = parseNode(meshes, materialInstances, gltf, *ecs, rootIndex, sceneRoot);

            updateTransformHierarchy(rootNode, glm::mat4{1.0f});
        }
    }

    // BREAKPOINT;
}

void Scene::updateTransformHierarchy(ECS::Entity entity, glm::mat4 parentToWorld)
{
    ECS& ecs = entity.getECS();

    auto* transform = entity.getComponent<Transform>();
    transform->localToWorld = parentToWorld * transform->localTransform;

    auto* hierarchy = entity.getComponent<Hierarchy>();
    for(auto& childID : hierarchy->children)
    {
        updateTransformHierarchy(ecs.getEntity(childID), transform->localToWorld);
    }
}