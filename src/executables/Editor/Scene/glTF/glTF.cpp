#include "glTFJSON.hpp"
#include <daw/daw_read_file.h>

static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::sint8) == 1);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::uint8) == 1);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::sint16) == 2);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::uint16) == 2);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::uint32) == 4);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::f32) == 4);

glTF::Main glTF::Main::load(std::string path)
{
    auto data = *daw::read_file(path);

    return daw::json::from_json<glTF::Main>(std::string_view(data.data(), data.size()));
}