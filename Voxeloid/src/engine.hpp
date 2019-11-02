#pragma once

#include "renderer.hpp"

#include <vulkan/vulkan.hpp>

struct GLFWwindow;

#define THROW_RUNTIME_ERROR(msg) \
	throw std::runtime_error(__FUNCTION__ ":\n\t" msg)

#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYERS = false;
#else
const bool ENABLE_VALIDATION_LAYERS = true;
#endif

class Engine
{
public:
	void init();
	void update();
	void render();

	void cleanup();

	bool IsRunning();

private:
	void initWindow();
	void createInstance();

	int window_width = 0;
	int window_height = 0;

	GLFWwindow* window = nullptr;


	vk::Instance vk_instance;
};