#include "glTFJSON.hpp"
#include <Engine/Misc/Macros.hpp>
#include <daw/daw_read_file.h>

static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::sint8) == 1);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::uint8) == 1);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::sint16) == 2);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::uint16) == 2);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::uint32) == 4);
static_assert(glTF::Accessor::getComponentTypeSize(glTF::Accessor::ComponentType::f32) == 4);

glTF::Main glTF::Main::load(std::string path)
{
    const std::string data = *daw::read_file(path);

#ifdef NDEBUG
    return daw::json::from_json<glTF::Main>(std::string_view(data.data(), data.size()));
#else
    glTF::Main gltf;
    try
    {
        gltf = daw::json::from_json<glTF::Main>(std::string_view(data.data(), data.size()));
    } catch(const daw::json::json_exception& ex)
    {
        std::string errorReason = ex.reason();
        std::string_view error_loc{ex.parse_location(), 50};
        std::cout << "Error parsing glTF file:\n" << errorReason << "\n" << error_loc << std::endl;
        BREAKPOINT;
    }
    return gltf;
#endif
}