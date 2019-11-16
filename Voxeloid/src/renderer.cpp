#include "renderer.hpp"

#include "util/runtimeerror.hpp"

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <iostream>
#include <vulkan/vulkan.hpp>

namespace
{
const std::vector<const char*> validation_layers = {
    "VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> device_extensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
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
    std::cerr << "VL: " << pCallbackData->pMessage << "\n" << std::endl;
    return VK_FALSE;
}

vk::DebugUtilsMessengerCreateInfoEXT createDebugMessengerCreateInfo()
{
    vk::DebugUtilsMessengerCreateInfoEXT create_info = {};
    vk::DebugUtilsMessageSeverityFlagsEXT ms;
    ms |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;
    ms |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo;
    ms |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose;
    ms |= vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning;
    create_info.messageSeverity = ms;

    vk::DebugUtilsMessageTypeFlagsEXT mt;
    mt |= vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral;
    mt |= vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;
    mt |= vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation;
    create_info.messageType = mt;
    create_info.pfnUserCallback = debugCallback;
    create_info.pUserData = nullptr;
    return create_info;
}

bool checkDeviceExtensionSupport(vk::PhysicalDevice device)
{
    auto available_extensions = device.enumerateDeviceExtensionProperties();

    std::set<std::string> required_extension(device_extensions.begin(),
                                             device_extensions.end());

    for (const auto& extension : available_extensions)
    {
        required_extension.erase(extension.extensionName);
    }
    return required_extension.empty();
}

vk::SurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<vk::SurfaceFormatKHR>& available_formats)
{
    for (const auto& format : available_formats)
    {

        if (format.format == vk::Format::eB8G8R8A8Unorm &&
            format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear)
        {
            return format;
        }
    }
    std::cout << "Warning: Choosing first surface format\n";
    return available_formats[0];
}

vk::PresentModeKHR chooseSwapPresentMode(
    const std::vector<vk::PresentModeKHR>& available_present_modes)
{
    for (const auto& present_mode : available_present_modes)
    {
        if (present_mode == vk::PresentModeKHR::eMailbox)
        {
            return present_mode;
        }
    }
    return vk::PresentModeKHR::eFifo;
}

} // namespace

void Renderer::init()
{
    window_width = 800;
    window_height = 600;

    initWindow();
    createInstance();
    if (ENABLE_VALIDATION_LAYERS)
    {
        setupDebugMessenger();
    }
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createSwapChain();
    createImageViews();
	createGraphicsPipeline();
}

void Renderer::cleanup()
{
	for (auto& image_view : swap_chain_image_views)
	{
		device.destroyImageView(image_view);
	}

    device.destroySwapchainKHR(swap_chain);
    device.destroy();

    if (ENABLE_VALIDATION_LAYERS)
    {
        vk_instance.destroyDebugUtilsMessengerEXT(
            debug_messenger, nullptr, dldi);
    }
    vkDestroySurfaceKHR(vk_instance, surface, nullptr);
    vk_instance.destroy();

    glfwDestroyWindow(window);
    glfwTerminate();
}

void Renderer::updateWindow() { glfwPollEvents(); }

bool Renderer::isRunning() { return !glfwWindowShouldClose(window); }

void Renderer::initWindow()
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

void Renderer::createInstance()
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

    auto debug_create_info = createDebugMessengerCreateInfo();
    if (ENABLE_VALIDATION_LAYERS)
    {
        instance_create_info.enabledLayerCount = validation_layers.size();
        instance_create_info.ppEnabledLayerNames = validation_layers.data();
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        instance_create_info.pNext = &debug_create_info;
    }
    instance_create_info.enabledExtensionCount = extensions.size();
    instance_create_info.ppEnabledExtensionNames = extensions.data();

    vk_instance = vk::createInstance(instance_create_info);

    dldi.init(vk_instance);
}

void Renderer::setupDebugMessenger()
{
    auto create_info = createDebugMessengerCreateInfo();

    debug_messenger =
        vk_instance.createDebugUtilsMessengerEXT(create_info, nullptr, dldi);
}

void Renderer::createSurface()
{
    VkSurfaceKHR c_surface;
    if (glfwCreateWindowSurface(vk_instance, window, nullptr, &c_surface) !=
        VK_SUCCESS)
    {
        THROW_RUNTIME_ERROR("Could not create glfw window surface")
    }
    surface = c_surface;
}

void Renderer::pickPhysicalDevice()
{
    auto devices = vk_instance.enumeratePhysicalDevices();
    if (devices.empty())
    {
        THROW_RUNTIME_ERROR("Failed to find physical devices");
    }

    for (const auto& device : devices)
    {
        if (isDeviceSuitable(device))
        {
            physical_device = device;
            break;
        }
    }
    if (physical_device == VK_NULL_HANDLE)
    {
        THROW_RUNTIME_ERROR("Failed to find suitable physical device");
    }
    //std::cout << "Debug: Picket physical device: '" << physical_device.getProperties().deviceName << "\n";
}

void Renderer::createLogicalDevice()
{
    QueueFamilyIndices indices = findQueueFamilies(physical_device);

    auto unique_queue_families = indices.getUniqueFamilies();

    float queue_priority = 1.0f;
    std::vector<vk::DeviceQueueCreateInfo> queue_create_infos;
    for (auto family : unique_queue_families)
    {
        vk::DeviceQueueCreateInfo queue_create_info;
        queue_create_info.queueFamilyIndex = indices.graphics.value();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.push_back(queue_create_info);
    }

    vk::PhysicalDeviceFeatures device_features;

    vk::DeviceCreateInfo device_create_info;
    device_create_info.pQueueCreateInfos = queue_create_infos.data();
    device_create_info.queueCreateInfoCount = queue_create_infos.size();
    device_create_info.pEnabledFeatures = &device_features;

    device_create_info.ppEnabledExtensionNames = device_extensions.data();
    device_create_info.enabledExtensionCount = device_extensions.size();

    if (ENABLE_VALIDATION_LAYERS)
    {
        device_create_info.enabledLayerCount = validation_layers.size();
        device_create_info.ppEnabledLayerNames = validation_layers.data();
    }

    device = physical_device.createDevice(device_create_info);

    graphics_queue = device.getQueue(indices.graphics.value(), 0);
    present_queue = device.getQueue(indices.present.value(), 0);
}

void Renderer::createSwapChain()
{
    SwapChainSupportDetails swap_chain_support =
        querySwapChainSupport(physical_device);

    auto surface_format = chooseSwapSurfaceFormat(swap_chain_support.formats);
    auto present_mode = chooseSwapPresentMode(swap_chain_support.present_modes);
    auto extent = chooseSwapExtent(swap_chain_support.capabilities);

    uint32_t image_count = swap_chain_support.capabilities.minImageCount + 1;

    if (swap_chain_support.capabilities.maxImageCount > 0 &&
        image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    vk::SwapchainCreateInfoKHR create_info;
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;

    QueueFamilyIndices indices = findQueueFamilies(physical_device);
    uint32_t queue_family_indices[] = { indices.graphics.value(),
                                        indices.present.value() };
    if (indices.graphics != indices.present)
    {
        create_info.imageSharingMode = vk::SharingMode::eConcurrent;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode = vk::SharingMode::eExclusive;
    }

    create_info.preTransform = swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = nullptr;

    swap_chain = device.createSwapchainKHR(create_info);

    swap_chain_images = device.getSwapchainImagesKHR(swap_chain);

    swap_chain_format = surface_format.format;
    swap_chain_extent = extent;
}

void Renderer::createImageViews()
{
    for (const auto& image : swap_chain_images)
    {
        vk::ImageViewCreateInfo create_info;
        create_info.image = image;
        create_info.viewType = vk::ImageViewType::e2D;
        create_info.format = swap_chain_format;
        create_info.components.r = vk::ComponentSwizzle::eIdentity;
        create_info.components.g = vk::ComponentSwizzle::eIdentity;
        create_info.components.b = vk::ComponentSwizzle::eIdentity;
        create_info.components.a = vk::ComponentSwizzle::eIdentity;
        create_info.subresourceRange.aspectMask =
            vk::ImageAspectFlagBits::eColor;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        auto view = device.createImageView(create_info);
        swap_chain_image_views.push_back(view);
    }
}

void Renderer::createGraphicsPipeline()
{

}

QueueFamilyIndices Renderer::findQueueFamilies(vk::PhysicalDevice device)
{
    QueueFamilyIndices indices;

    auto properties = device.getQueueFamilyProperties();

    int i = 0;
    for (const auto& property : properties)
    {
        if (property.queueFlags & vk::QueueFlagBits::eGraphics)
        {
            indices.graphics = i;
        }

        if (device.getSurfaceSupportKHR(i, surface))
        {
            indices.present = i;
        }

        if (indices.isComplete())
        {
            break;
        }

        i++;
    }

    return indices;
}

bool Renderer::isDeviceSuitable(vk::PhysicalDevice device)
{
    auto indices = findQueueFamilies(device);

    bool extensions_supported = checkDeviceExtensionSupport(device);

    bool swap_chain_sufficient = false;
    if (extensions_supported)
    {
        auto swap_chain_support = querySwapChainSupport(device);
        swap_chain_sufficient = !swap_chain_support.formats.empty() &&
                                !swap_chain_support.present_modes.empty();
    }

    return indices.isComplete() && extensions_supported &&
           swap_chain_sufficient;
}

SwapChainSupportDetails
    Renderer::querySwapChainSupport(vk::PhysicalDevice device)
{
    SwapChainSupportDetails details;

    details.capabilities = device.getSurfaceCapabilitiesKHR(surface);
    details.formats = device.getSurfaceFormatsKHR(surface);
    details.present_modes = device.getSurfacePresentModesKHR(surface);

    return details;
}

vk::Extent2D
    Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        vk::Extent2D actual_extent = { window_width, window_height };

        actual_extent.width = glm::clamp(actual_extent.width,
                                         capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width);
        actual_extent.height = glm::clamp(actual_extent.height,
                                          capabilities.minImageExtent.height,
                                          capabilities.maxImageExtent.height);
        return actual_extent;
    }
}
