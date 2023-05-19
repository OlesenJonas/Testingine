#include "glTFJSON.hpp"
#include <daw/daw_read_file.h>

const glTF::Main glTF::Main::load(std::string path)
{
    auto data = *daw::read_file(path);

    return daw::json::from_json<glTF::Main>(std::string_view(data.data(), data.size()));
}