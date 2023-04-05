#include <intern/Engine/Engine.hpp>

int main()
{
    Engine engine;

    while(engine.isRunning())
    {
        // will probably split when accessing specific parts of the update is needed
        // but thats not the case atm
        engine.update();
    }

    return 0;
}