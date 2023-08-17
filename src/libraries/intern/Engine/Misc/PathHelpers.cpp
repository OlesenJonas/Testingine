#include "PathHelpers.hpp"

namespace PathHelpers
{
    std::string_view fileName(std::string_view path)
    {
        auto lastDirSep = path.find_last_of("/\\");
        auto extensionStart = path.find_last_of('.');

        return path.substr(lastDirSep + 1, extensionStart - lastDirSep - 1);
    }

    std::string_view extension(std::string_view path)
    {
        auto extensionStart = path.find_last_of('.');
        return path.substr(extensionStart);
    }

} // namespace PathHelpers