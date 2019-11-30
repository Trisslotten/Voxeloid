#include "engine.hpp"

#include <iostream>
#include "voxeloctree.hpp"
#include "util/timer.hpp"


int main()
{
	Timer t;
	t.Restart();
	VoxelOctree svo;
	double time = t.Elapsed();

	std::cout << "Elapsed: " << time << "\n";

	/*
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
	*/

	system("pause");

    return EXIT_SUCCESS;
}