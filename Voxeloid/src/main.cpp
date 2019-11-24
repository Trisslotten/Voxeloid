#include <iostream>

#include "engine.hpp"

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

	//system("pause");

	return EXIT_SUCCESS;
}