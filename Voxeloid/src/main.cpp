#include <iostream>

#include "engine.hpp"

int main()
{
	try
	{
		Engine engine;
		engine.init();

		while (engine.IsRunning())
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