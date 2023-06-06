#pragma once

#include "glTF.hpp"
#include <daw/json/daw_json_link.h>

template <typename T, T defaultValue>
struct ValueOrDefault
{
    inline T operator()()
    {
        return defaultValue;
    }
    inline T operator()(T value)
    {
        return value;
    }
};

namespace daw::json
{
    template <JSONNAMETYPE Name, typename T, T defaultValue>
    using json_number_or_default = json_number_null<
        Name,                             //
        T,                                //
        number_opts_def,                  //
        JsonNullable::Nullable,           //
        ValueOrDefault<T, defaultValue>>; //
};

namespace daw::json
{
    template <JSONNAMETYPE Name, typename T, T defaultValue>
        requires std::is_default_constructible_v<T>
    using json_class_or_default = json_class_null< //
        Name,                                      //
        T,                                         //
        JsonNullable::Nullable,                    //
        ValueOrDefault<T, defaultValue>>;          //
};

#define JSONType(T)                                                                                               \
    template <>                                                                                                   \
    struct daw::json::json_data_contract<T>

JSONType(glm::vec3)
{
    using type = json_ordered_member_list<float, float, float>;
};

JSONType(glm::vec4)
{
    using type = json_ordered_member_list<float, float, float, float>;
};

JSONType(glTF::AssetInfo)
{
    using type = json_member_list<json_string<"version">>;
};

JSONType(glTF::Scene)
{
    using type = json_member_list<json_array<"nodes", int>>;
};

JSONType(glTF::Node)
{
    using type = json_member_list<                                                   //
        json_number_null<"mesh", std::optional<int>>,                                //
        json_array_null<"children", std::vector<int>>,                               //
        json_class_or_default<"translation", glm::vec3, glm::vec3{0.0f}>,            //
        json_class_or_default<"rotation", glm::vec4, glm::vec4{0.f, 0.f, 0.f, 1.f}>, //
        json_class_or_default<"scale", glm::vec3, glm::vec3{1.f, 1.f, 1.f}>          //
        >;
};

JSONType(glTF::PrimitiveAttributes)
{
    using type = json_member_list<                      //
        json_number<"NORMAL", int>,                     //
        json_number<"POSITION", int>,                   //
        json_number<"TEXCOORD_0", int>,                 //
        json_number_null<"TANGENT", std::optional<int>> //
        >;
};

JSONType(glTF::Primitive)
{
    using type = json_member_list<                           //
        json_class<"attributes", glTF::PrimitiveAttributes>, //
        json_number_null<"indices", std::optional<int>>,     //
        json_number<"material", int>                         //
        >;
};

JSONType(glTF::Mesh)
{
    using type = json_member_list<                 //
        json_array<"primitives", glTF::Primitive>, //
        json_string<"name">                        //
        >;
};

JSONType(glTF::Accessor)
{
    using type = json_member_list<                         //
        json_number<"bufferView", uint32_t>,               //
        json_number_or_default<"byteOffset", uint32_t, 0>, //
        json_number<"componentType", uint32_t>,            //
        json_number<"count", uint32_t>,                    //
        json_string<"type">                                //
        >;
};

JSONType(glTF::TextureParams)
{
    using type = json_member_list<     //
        json_number<"index", uint32_t> //
        >;
};

JSONType(glTF::PBRMetalRoughParams)
{
    using type = json_member_list<                                  //
        json_class<"baseColorTexture", glTF::TextureParams>,        //
        json_class<"metallicRoughnessTexture", glTF::TextureParams> //
        >;
};

JSONType(glTF::Material)
{
    using type = json_member_list<                                     //
        json_class<"pbrMetallicRoughness", glTF::PBRMetalRoughParams>, //
        json_class<"occlusionTexture", glTF::TextureParams>,           //
        json_class<"normalTexture", glTF::TextureParams>               //
        >;
};

JSONType(glTF::Texture)
{
    using type = json_member_list<        //
        json_number<"sampler", uint32_t>, //
        json_number<"source", uint32_t>   //
        >;
};

JSONType(glTF::Image)
{
    using type = json_member_list< //
        json_string<"uri">         //
        >;
};

JSONType(glTF::Sampler)
{
    using type = json_member_list<          //
        json_number<"magFilter", uint32_t>, //
        json_number<"minFilter", uint32_t>, //
        json_number<"wrapS", uint32_t>,     //
        json_number<"wrapT", uint32_t>      //
        >;
};

JSONType(glTF::BufferView)
{
    using type = json_member_list<                         //
        json_number<"buffer", uint32_t>,                   //
        json_number_or_default<"byteOffset", uint32_t, 0>, //
        json_number<"byteLength", uint32_t>,               //
        json_number_or_default<"byteStride", uint32_t, 0>  //
        >;
};

JSONType(glTF::Buffer)
{
    using type = json_member_list<           //
        json_number<"byteLength", uint32_t>, //
        json_string<"uri">                   //
        >;
};

JSONType(glTF::Main)
{
    using type = json_member_list<                   //
        json_class<"asset", glTF::AssetInfo>,        //
        json_number<"scene", int>,                   //
        json_array<"scenes", glTF::Scene>,           //
        json_array<"nodes", glTF::Node>,             //
        json_array<"meshes", glTF::Mesh>,            //
        json_array<"accessors", glTF::Accessor>,     //
        json_array<"materials", glTF::Material>,     //
        json_array<"textures", glTF::Texture>,       //
        json_array<"images", glTF::Image>,           //
        json_array<"samplers", glTF::Sampler>,       //
        json_array<"bufferViews", glTF::BufferView>, //
        json_array<"buffers", glTF::Buffer>          //
        >;
};