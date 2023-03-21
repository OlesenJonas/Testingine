#include "Engine.hpp"

Engine::Engine()
{
    ptr = this;

    renderer.init();
    mainCamera = Camera{
        static_cast<float>(renderer.windowExtent.width) / static_cast<float>(renderer.windowExtent.height),
        0.1f,
        1000.0f};
    inputManager.init();
}

Engine::~Engine()
{
    renderer.cleanup();
}

void Engine::run()
{
    renderer.run();
}