#include <intern/Graphics/VulkanRenderer.hpp>

int main()
{
    VulkanRenderer renderer;

    renderer.init();

    renderer.run();

    renderer.cleanup();

    return 0;
}