#include "DefaultComponents.hpp"
#include "Scene.hpp"
#include "glTF/glTF.hpp"
#include "glTF/glTFtoTI.hpp"

#include <Datastructures/Pool/Pool.hpp>
#include <Engine/Application/Application.hpp>
#include <Engine/ResourceManager/ResourceManager.hpp>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <vulkan/vulkan_core.h>

#include <daw/json/daw_json_exception.h>

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
    std::vector<std::array<Mesh::Handle, Mesh::MAX_SUBMESHES>> meshes;
    meshes.resize(gltf.meshes.size(), FilledArray<Mesh::Handle, Mesh::MAX_SUBMESHES>(Mesh::Handle::Invalid()));
    for(int i = 0; i < gltf.meshes.size(); i++)
    {
        // not just using the bufferView/Accessors as defined by gltf
        // instead transforming the data so everything fits a unified mesh layout

        const glTF::Mesh& mesh = gltf.meshes[i];

        if(mesh.primitives.size() > Mesh::MAX_SUBMESHES)
        {
            BREAKPOINT;
            // TODO: warn more than max allowed submeshes
        }

        for(int prim = 0; prim < glm::min<int>(Mesh::MAX_SUBMESHES, mesh.primitives.size()); prim++)
        {
            const auto& primitive = mesh.primitives[prim];

            const glTF::Accessor& positionAccessor = gltf.accessors[primitive.attributes.positionAccessor];
            const glTF::Accessor& normalAccessor = gltf.accessors[primitive.attributes.normalAccessor];
            const glTF::Accessor& uv0Accessor = gltf.accessors[primitive.attributes.uv0Accessor];
            // todo: load vertex colors (if available)
            const uint32_t vertexCount = positionAccessor.count;
            assert(normalAccessor.count == vertexCount);
            assert(uv0Accessor.count == vertexCount);
            Mesh::VertexAttributeFormat attribFormat{.additionalUVCount = 0};
            const glTF::Accessor* uv1Accessor = nullptr;
            const glTF::Accessor* uv2Accessor = nullptr;
            if(primitive.attributes.uv1Accessor.has_value())
            {
                attribFormat.additionalUVCount++;
                uv1Accessor = &gltf.accessors[primitive.attributes.uv1Accessor.value()];
            }
            if(primitive.attributes.uv2Accessor.has_value())
            {
                attribFormat.additionalUVCount++;
                uv2Accessor = &gltf.accessors[primitive.attributes.uv2Accessor.value()];
            }

            std::vector<Mesh::PositionType> vertexPositions;
            vertexPositions.resize(vertexCount);
            std::vector<std::byte> vertexAttributes;
            size_t vertexAttributeSize = attribFormat.combinedSize();
            vertexAttributes.resize(vertexCount * vertexAttributeSize);
            const size_t attribStride = vertexAttributeSize;
            const size_t normalOffset = attribFormat.normalOffset();
            const size_t colorOffset = attribFormat.colorOffset();
            const size_t uv0Offset = attribFormat.uvOffset(0);
            const size_t uv1Offset = attribFormat.uvOffset(1);
            const size_t uv2Offset = attribFormat.uvOffset(2);

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
                char* startAddress =
                    &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

                // normals must be of type float3
                assert(accessor.componentType == glTF::Accessor::f32);
                assert(accessor.type == glTF::Accessor::vec3);
                auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 3;

                for(int j = 0; j < vertexCount; j++)
                {
                    auto* targetAddr = (glm::vec3*)(&(vertexAttributes[j * attribStride + normalOffset]));
                    *targetAddr = *((glm::vec3*)(startAddress + static_cast<size_t>(j * effectiveStride)));
                }
            }

            // read uvs
            // 0
            {
                const glTF::Accessor& accessor = uv0Accessor;
                const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
                char* startAddress =
                    &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

                // uvs must be of type float2
                assert(accessor.componentType == glTF::Accessor::f32);
                assert(accessor.type == glTF::Accessor::vec2);
                auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 2;

                for(int j = 0; j < vertexCount; j++)
                {
                    auto* targetAddr = (glm::vec2*)(&(vertexAttributes[j * attribStride + uv0Offset]));
                    *targetAddr = *((glm::vec2*)(startAddress + static_cast<size_t>(j * effectiveStride)));
                }
            }
            // 1
            if(uv1Accessor != nullptr)
            {
                const glTF::Accessor& accessor = *uv1Accessor;
                const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
                char* startAddress =
                    &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

                // uvs must be of type float2
                assert(accessor.componentType == glTF::Accessor::f32);
                assert(accessor.type == glTF::Accessor::vec2);
                auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 2;

                for(int j = 0; j < vertexCount; j++)
                {
                    auto* targetAddr = (glm::vec2*)(&(vertexAttributes[j * attribStride + uv1Offset]));
                    *targetAddr = *((glm::vec2*)(startAddress + static_cast<size_t>(j * effectiveStride)));
                }
            }
            // 2
            if(uv2Accessor != nullptr)
            {
                const glTF::Accessor& accessor = *uv2Accessor;
                const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
                char* startAddress =
                    &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

                // uvs must be of type float2
                assert(accessor.componentType == glTF::Accessor::f32);
                assert(accessor.type == glTF::Accessor::vec2);
                auto effectiveStride = bufferView.byteStride != 0 ? bufferView.byteStride : sizeof(float) * 2;

                for(int j = 0; j < vertexCount; j++)
                {
                    auto* targetAddr = (glm::vec2*)(&(vertexAttributes[j * attribStride + uv2Offset]));
                    *targetAddr = *((glm::vec2*)(startAddress + static_cast<size_t>(j * effectiveStride)));
                }
            }

            std::vector<uint32_t> indices;
            if(primitive.indexAccessor.has_value())
            {
                const glTF::Accessor& accessor = gltf.accessors[primitive.indexAccessor.value()];
                indices.resize(accessor.count);

                const glTF::BufferView& bufferView = gltf.bufferViews[accessor.bufferViewIndex];
                char* startAddress =
                    &(buffers[bufferView.bufferIndex][bufferView.byteOffset + accessor.byteOffset]);

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

            // Tangents are no longer loaded
            // ...

            Mesh::Handle newMesh = rm->createMesh(
                vertexPositions,
                vertexAttributes,
                attribFormat,
                indices,
                mesh.name + "_sub" + std::to_string(prim));
            meshes[i][prim] = newMesh;
        }
    }

    // Create material instances (materials in glTF)
    std::vector<MaterialInstance::Handle> materialInstances;
    materialInstances.resize(gltf.materials.size());
    auto basicPBRMaterial = rm->getMaterial("PBRBasic");
    assert(rm->get<std::string>(basicPBRMaterial) != nullptr);
    for(int i = 0; i < gltf.materials.size(); i++)
    {
        const glTF::Material& material = gltf.materials[i];
        auto matInst = rm->createMaterialInstance(basicPBRMaterial);

        const glTF::Texture& baseColorTextureGLTF =
            gltf.textures[material.pbrMetallicRoughness.baseColorTexture.index];
        const Texture::Handle baseColorTextureHandle = textures[baseColorTextureGLTF.sourceIndex];
        const Handle<Sampler> baseColorSamplerHandle = samplers[baseColorTextureGLTF.samplerIndex];
        Sampler* baseColorSampler = rm->get(baseColorSamplerHandle);
        MaterialInstance::setResource(
            matInst, "baseColorTexture", *rm->get<ResourceIndex>(baseColorTextureHandle));
        // MaterialInstance::setResource(matInst, "baseColorSampler", baseColorSampler->sampler.resourceIndex);

        if(material.normalTexture.has_value())
        {
            const glTF::Texture& normalTextureGLTF = gltf.textures[material.normalTexture.value().index];
            const Texture::Handle normalTextureHandle = textures[normalTextureGLTF.sourceIndex];
            const Handle<Sampler> normalSamplerHandle = samplers[normalTextureGLTF.samplerIndex];
            Sampler* normalSampler = rm->get(normalSamplerHandle);
            MaterialInstance::setResource(matInst, "normalTexture", *rm->get<ResourceIndex>(normalTextureHandle));
            // MaterialInstance::setResource(matInst, "normalSampler", normalSampler->sampler.resourceIndex);
        }
        else
        {
            MaterialInstance::setResource(matInst, "normalTexture", 0xFFFFFFFF);
            // MaterialInstance::setResource(matInst, "normalSampler", 0xFFFFFFFF);
        }

        MaterialInstance::setFloat(matInst, "metallicFactor", material.pbrMetallicRoughness.metallicFactor);
        MaterialInstance::setFloat(matInst, "roughnessFactor", material.pbrMetallicRoughness.roughnessFactor);

        if(material.pbrMetallicRoughness.metallicRoughnessTexture.has_value())
        {
            const glTF::Texture& metalRoughTextureGLTF =
                gltf.textures[material.pbrMetallicRoughness.metallicRoughnessTexture.value().index];
            const Texture::Handle metalRoughTextureHandle = textures[metalRoughTextureGLTF.sourceIndex];
            const Handle<Sampler> metalRoughSamplerHandle = samplers[metalRoughTextureGLTF.samplerIndex];
            Sampler* metalRoughSampler = rm->get(metalRoughSamplerHandle);
            MaterialInstance::setResource(
                matInst, "metalRoughTexture", *rm->get<ResourceIndex>(metalRoughTextureHandle));
            // MaterialInstance::setResource(matInst, "metalRoughSampler",
            // metalRoughSampler->sampler.resourceIndex);
        }
        else
        {
            MaterialInstance::setResource(matInst, "metalRoughTexture", 0xFFFFFFFF);
            // MaterialInstance::setResource(matInst, "metalRoughSampler", 0xFFFFFFFF);
        }

        if(material.occlusionTexture.has_value())
        {
            const glTF::Texture& occlusionTextureGLTF = gltf.textures[material.occlusionTexture.value().index];
            const Texture::Handle occlusionTextureHandle = textures[occlusionTextureGLTF.sourceIndex];
            const Handle<Sampler> occlusionSamplerHandle = samplers[occlusionTextureGLTF.samplerIndex];
            Sampler* occlusionSampler = rm->get(occlusionSamplerHandle);
            MaterialInstance::setResource(
                matInst, "occlusionTexture", *rm->get<ResourceIndex>(occlusionTextureHandle));
            // MaterialInstance::setResource(matInst, "occlusionSampler", occlusionSampler->sampler.resourceIndex);
        }
        else
        {
            MaterialInstance::setResource(matInst, "occlusionTexture", 0xFFFFFFFF);
            // MaterialInstance::setResource(matInst, "occlusionSampler", 0xFFFFFFFF);
        }

        materialInstances[i] = matInst;
    }

    std::vector<ECS::Entity> gltfNodes;
    gltfNodes.reserve(gltf.nodes.size());

    // Create all nodes
    for(int i = 0; i < gltf.nodes.size(); i++)
    {
        const glTF::Node& glTFNode = gltf.nodes[i];

        ECS::Entity& nodeEntity = gltfNodes.emplace_back(ecs->createEntity());

        auto* nodeTransform = nodeEntity.addComponent<Transform>();
        nodeTransform->position = glTFNode.translation;
        nodeTransform->orientation = glm::quat{
            glTFNode.rotationAsVec.w,
            glTFNode.rotationAsVec.x,
            glTFNode.rotationAsVec.y,
            glTFNode.rotationAsVec.z};
        nodeTransform->scale = glTFNode.scale;
        nodeTransform->calculateLocalTransformMatrix();

        if(glTFNode.meshIndex.has_value())
        {
            auto* renderInfo = nodeEntity.addComponent<MeshRenderer>();
            const glTF::Mesh& gltfMesh = gltf.meshes[glTFNode.meshIndex.value()];
            renderInfo->subMeshes = meshes[glTFNode.meshIndex.value()];
            for(int i = 0; i < gltfMesh.primitives.size(); i++)
            {
                renderInfo->materialInstances[i] = materialInstances[gltfMesh.primitives[i].materialIndex];
            }
        }

        auto* nodeHierarchy = nodeEntity.addComponent<Hierarchy>();
    }
    assert(gltfNodes.size() == gltf.nodes.size());

    // link up hierarchies
    // From scene parents to nodes
    std::vector<ECS::Entity> sceneRoots;
    sceneRoots.reserve(gltf.scenes.size());
    for(int i = 0; i < gltf.scenes.size(); i++)
    {
        const glTF::Scene& scene = gltf.scenes[i];
        ECS::Entity& sceneRoot = sceneRoots.emplace_back(ecs->createEntity());
        sceneRoot.addComponent<Transform>();
        auto* rootHierarchy = sceneRoot.addComponent<Hierarchy>();
        rootHierarchy->parent = parent;
        parent.getComponent<Hierarchy>()->children.push_back(sceneRoot);

        for(const int& child : scene.nodeIndices)
        {
            ECS::Entity childNode = gltfNodes[child];
            rootHierarchy->children.push_back(childNode);
            auto* childHierarchy = childNode.getComponent<Hierarchy>();
            childHierarchy->parent = sceneRoot;
        }
    }
    // From nodes to other nodes
    for(int i = 0; i < gltf.nodes.size(); i++)
    {
        const glTF::Node& gltfParent = gltf.nodes[i];
        ECS::Entity& parentEntity = gltfNodes[i];
        auto* parentHierarchy = parentEntity.getComponent<Hierarchy>();

        if(gltfParent.childNodeIndices.has_value())
        {
            for(const int& child : gltfParent.childNodeIndices.value())
            {
                ECS::Entity childEntity = gltfNodes[child];
                parentHierarchy->children.push_back(childEntity);
                auto* childHierarchy = childEntity.getComponent<Hierarchy>();
                childHierarchy->parent = parentEntity;
            }
        }
    }

    updateTransformHierarchy(parent, glm::mat4{1.0f});
    // BREAKPOINT;
}

void Scene::updateTransformHierarchy(ECS::Entity entity, glm::mat4 parentToWorld)
{
    ECS& ecs = *ECS::impl();

    auto* transform = entity.getComponent<Transform>();
    assert(transform);
    transform->localToWorld = parentToWorld * transform->localTransform;

    auto* hierarchy = entity.getComponent<Hierarchy>();
    for(ECS::Entity& child : hierarchy->children)
    {
        updateTransformHierarchy(child, transform->localToWorld);
    }
}