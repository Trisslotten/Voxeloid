#include "engine.hpp"
#include "util/timer.hpp"
#include "voxeloctree.hpp"

#include <iostream>

int main()
{
    try
    {
        Engine engine;
        engine.init();

        while (engine.isRunning())
        {
            engine.update();
        }
        engine.cleanup();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return EXIT_SUCCESS;
}