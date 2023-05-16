#include "Scene.hpp"
#include <ECS/ECS.hpp>
#include <Engine/Engine.hpp>

#include <daw/daw_read_file.h>
#include <daw/json/daw_json_link.h>

SceneObject::SceneObject(ECS::Entity entity) : entity(entity){};
Renderable::Renderable(ECS::Entity entity) : SceneObject(entity){};

Scene::Scene(ECS& ecs) : ecs(ecs), root(ecs.createEntity())
{
    ecs.registerComponent<Transform>();
    ecs.registerComponent<Hierarchy>();
    ecs.registerComponent<RenderInfo>();

    Transform& rootTransform = *root.addComponent<Transform>();
    rootTransform.calculateLocalTransformMatrix();
    root.addComponent<Hierarchy>();
}

Transform* SceneObject::getTransform()
{
    return entity.getComponent<Transform>();
}

Renderable Scene::addRenderable()
{
    Renderable renderable{ecs.createEntity()};
    renderable.entity.addComponent<Transform>();
    renderable.entity.addComponent<Hierarchy>();
    renderable.entity.addComponent<RenderInfo>();

    return renderable;
}

RenderInfo* Renderable::getRenderInfo()
{
    return entity.getComponent<RenderInfo>();
}

// todo: move somewhere else!
namespace glTFJson
{
    struct AssetInfo
    {
        // can ignore the first two, not relevent atm
        std::string copyright;
        std::string generator;
        std::string version;
    };
    struct Main
    {
        AssetInfo asset;
    };
} // namespace glTFJson

namespace daw::json
{
    template <>
    struct json_data_contract<glTFJson::AssetInfo>
    {
        using type = json_member_list<json_string<"copyright">, json_string<"generator">, json_string<"version">>;
    };
    // static inline auto to_json_data(glTFJson::AssetInfo const& value)
    // {
    //     return std::forward_as_tuple(value.copyright, value.generator, value.version);
    // }
    template <>
    struct json_data_contract<glTFJson::Main>
    {
        using type = json_member_list<json_class<"asset", glTFJson::AssetInfo>>;
    };

} // namespace daw::json

void Scene::load(std::string path)
{
    auto data = *daw::read_file(path);

    auto const cls = daw::json::from_json<glTFJson::Main>(std::string_view(data.data(), data.size()));

    int x = 13;
}
