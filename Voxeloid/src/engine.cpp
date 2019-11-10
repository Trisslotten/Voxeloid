#include "engine.hpp"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vulkan/vulkan.hpp>

namespace
{
const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};

bool checkValidationLayerSupport()
{
    auto available_layers = vk::enumerateInstanceLayerProperties();

    bool all_layers_found = true;
    for (auto& wanted_layer : validation_layers)
    {
        bool found_current = false;
        for (auto& layer : available_layers)
        {
            if (strcmp(wanted_layer, layer.layerName) == 0)
            {
                found_current = true;
                break;
            }
        }
        if (!found_current)
        {
            all_layers_found = false;
            break;
        }
    }

    return all_layers_found;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
              void* pUserData)
{
    std::cerr << "Validation layer: " << pCallbackData->pMessage << std::endl;
    return VK_FALSE;
}

} // namespace

void Engine::init()
{
    window_width = 800;
    window_height = 600;

    initWindow();
    createInstance();
    setupDebugMessenger();
}

void Engine::update() { glfwPollEvents(); }

void Engine::render() {}

void Engine::cleanup()
{
    if (ENABLE_VALIDATION_LAYERS)
    {
        vk_instance.destroyDebugUtilsMessengerEXT(
            debug_messenger, nullptr, dldi);
    }

    vk_instance.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();
}

bool Engine::IsRunning() { return !glfwWindowShouldClose(window); }

void Engine::initWindow()
{
    if (!glfwInit())
    {
        THROW_RUNTIME_ERROR("Could not init glfw");
    }
    if (!glfwVulkanSupported())
    {
        THROW_RUNTIME_ERROR("Vulkan not supported");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    window = glfwCreateWindow(
        window_width, window_height, "Vulkan", nullptr, nullptr);
    if (!window)
    {
        THROW_RUNTIME_ERROR("Could not create glfw window");
    }
}

void Engine::createInstance()
{
    if (ENABLE_VALIDATION_LAYERS && !checkValidationLayerSupport())
    {
        THROW_RUNTIME_ERROR("Validation layers requested, but not available");
    }

    uint32_t glfw_extension_count = 0;
    const char** glfw_extensions;
    glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
    std::vector<const char*> extensions(glfw_extensions,
                                        glfw_extensions + glfw_extension_count);

    vk::ApplicationInfo app_info("EngineTest",
                                 VK_MAKE_VERSION(0, 1, 0),
                                 "Voxeloid",
                                 VK_MAKE_VERSION(0, 1, 0),
                                 VK_API_VERSION_1_1);

    vk::InstanceCreateInfo instance_create_info({}, &app_info);
    if (ENABLE_VALIDATION_LAYERS)
    {
        instance_create_info.enabledLayerCount = validation_layers.size();
        instance_create_info.ppEnabledLayerNames = validation_layers.data();
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    instance_create_info.enabledExtensionCount = extensions.size();
    instance_create_info.ppEnabledExtensionNames = extensions.data();

    vk_instance = vk::createInstance(instance_create_info);

    dldi.init(vk_instance);
}

void Engine::setupDebugMessenger()
{
    vk::DebugUtilsMessengerCreateInfoEXT create_info = {};
    create_info.messageSeverity =
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eError |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
        vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
    create_info.messageType =
        vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
        vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance |
        vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    create_info.pfnUserCallback = debugCallback;
    create_info.pUserData = nullptr;

    debug_messenger =
        vk_instance.createDebugUtilsMessengerEXT(create_info, nullptr, dldi);
}
