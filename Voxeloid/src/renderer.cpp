#include "renderer.hpp"

#include "util/runtimeerror.hpp"

#include <GLFW/glfw3.h>
#include <fstream>
#include <glm/ext.hpp>
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

static std::vector<char> readFile(const std::string& filename)
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);

    if (!file.is_open())
    {
        THROW_RUNTIME_ERROR("Failed to open file: '" + filename + "'");
    }
    size_t file_size = (size_t)file.tellg();
    std::vector<char> buffer(file_size);
    file.seekg(0);
    file.read(buffer.data(), file_size);
    file.close();
    return buffer;
}

} // namespace

void Renderer::init()
{
    window_width = 1600;
    window_height = 900;

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
    createRenderPass();
	createDescriptorSetLayout();
    createGraphicsPipeline();
    createFramebuffers();
    createCommandPool();
    createUniformBuffers();
	createDescriptorPool();
	createDescriptorSets();
    createCommandBuffers();
    createSyncObjects();
}

void Renderer::cleanup()
{
    device.waitIdle();

    for (size_t i = 0; i < swap_chain_images.size(); i++)
    {
        device.destroyBuffer(uniform_buffers[i]);
        device.freeMemory(uniform_buffers_memory[i]);
    }

	device.destroyDescriptorPool(descriptor_pool);

    for (auto fence : in_flight_fences)
        device.destroyFence(fence);
    for (auto semaphore : image_available_semaphores)
        device.destroySemaphore(semaphore);
    for (auto semaphore : render_finished_semaphores)
        device.destroySemaphore(semaphore);

    device.destroyCommandPool(command_pool);

    for (auto framebuffer : swap_chain_framebuffers)
    {
        device.destroyFramebuffer(framebuffer);
    }

    device.destroyPipeline(graphics_pipeline);

    device.destroyPipelineLayout(pipeline_layout);
    device.destroyRenderPass(render_pass);

    for (auto& image_view : swap_chain_image_views)
    {
        device.destroyImageView(image_view);
    }

    device.destroyDescriptorSetLayout(descriptor_set_layout);

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

void Renderer::render()
{
    device.waitForFences(in_flight_fences[current_frame], VK_TRUE, UINT64_MAX);

    auto result = device.acquireNextImageKHR(
        swap_chain, UINT64_MAX, image_available_semaphores[current_frame], {});
    uint32_t image_index = result.value;

    if (fences_used[image_index])
    {
        device.waitForFences(
            images_in_flight[image_index], VK_TRUE, UINT64_MAX);
    }
    images_in_flight[image_index] = in_flight_fences[current_frame];
    fences_used[image_index] = true;

    updateUniformBuffer(image_index);

    vk::SubmitInfo submit_info;

    vk::Semaphore wait_semaphores[] = {
        image_available_semaphores[current_frame]
    };
    vk::PipelineStageFlags wait_stages[] = {
        vk::PipelineStageFlagBits::eColorAttachmentOutput
    };
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffers[image_index];

    vk::Semaphore signal_semaphores[] = {
        render_finished_semaphores[current_frame]
    };
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    device.resetFences(in_flight_fences[current_frame]);
    graphics_queue.submit({ submit_info }, in_flight_fences[current_frame]);

    vk::PresentInfoKHR present_info;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    vk::SwapchainKHR swap_chains[] = { swap_chain };
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr; // Optional

    present_queue.presentKHR(present_info);

    fps_counter++;

    current_frame = (current_frame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::updateWindow()
{
    glfwPollEvents();

    dt = timer.Restart();

    if (fps_timer.Elapsed() > 1.0)
    {
        double elapsed = fps_timer.Restart();

        double fps = fps_counter / elapsed;

        std::string title = "Vulkan " + std::to_string(fps);
        glfwSetWindowTitle(window, title.c_str());

        fps_counter = 0;
    }
}

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

void Renderer::createRenderPass()
{
    vk::AttachmentDescription color_attachment;
    color_attachment.format = swap_chain_format;
    color_attachment.samples = vk::SampleCountFlagBits::e1;
    color_attachment.loadOp = vk::AttachmentLoadOp::eClear;
    color_attachment.storeOp = vk::AttachmentStoreOp::eStore;
    color_attachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
    color_attachment.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
    color_attachment.initialLayout = vk::ImageLayout::eUndefined;
    color_attachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

    vk::AttachmentReference color_attachment_ref;
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = vk::ImageLayout::eColorAttachmentOptimal;

    vk::SubpassDescription subpass;
    subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    vk::SubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    //dependency.srcAccessMask = 0;
    dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
    dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead |
                               vk::AccessFlagBits::eColorAttachmentWrite;

    vk::RenderPassCreateInfo render_pass_info;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    render_pass = device.createRenderPass(render_pass_info);
}

void Renderer::createDescriptorSetLayout()
{
    vk::DescriptorSetLayoutBinding ubo_layout_binding;
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = vk::DescriptorType::eUniformBuffer;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags =
        vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment;

    vk::DescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    descriptor_set_layout = device.createDescriptorSetLayout(layout_info);
}

void Renderer::createUniformBuffers()
{
    vk::DeviceSize buffer_size = 2 * sizeof(glm::vec4);

    uniform_buffers.resize(swap_chain_images.size());
    uniform_buffers_memory.resize(swap_chain_images.size());

    for (size_t i = 0; i < swap_chain_images.size(); i++)
    {
        createBuffer(buffer_size,
                     vk::BufferUsageFlagBits::eUniformBuffer,
                     vk::MemoryPropertyFlagBits::eHostVisible |
                         vk::MemoryPropertyFlagBits::eHostCoherent,
                     uniform_buffers[i],
                     uniform_buffers_memory[i]);
    }
}

void Renderer::createDescriptorPool()
{
	vk::DescriptorPoolSize pool_size;
	pool_size.descriptorCount = static_cast<uint32_t>(swap_chain_images.size());

	vk::DescriptorPoolCreateInfo pool_info;
	pool_info.poolSizeCount = 1;
	pool_info.pPoolSizes = &pool_size;
	pool_info.maxSets = static_cast<uint32_t>(swap_chain_images.size());

	descriptor_pool = device.createDescriptorPool(pool_info);
}

void Renderer::createDescriptorSets()
{
	std::vector<vk::DescriptorSetLayout> layouts(swap_chain_images.size(), descriptor_set_layout);
	vk::DescriptorSetAllocateInfo alloc_info;
	alloc_info.descriptorPool = descriptor_pool;
	alloc_info.descriptorSetCount = static_cast<uint32_t>(swap_chain_images.size());
	alloc_info.pSetLayouts = layouts.data();

	descriptor_sets = device.allocateDescriptorSets(alloc_info);

	for (size_t i = 0; i < swap_chain_images.size(); i++)
	{
		vk::DescriptorBufferInfo buffer_info;
		buffer_info.buffer = uniform_buffers[i];
		buffer_info.offset = 0;
		buffer_info.range = 2*sizeof(glm::vec4);

		vk::WriteDescriptorSet descriptor_write;
		descriptor_write.dstSet = descriptor_sets[i];
		descriptor_write.dstBinding = 0;
		descriptor_write.dstArrayElement = 0;
		descriptor_write.descriptorType = vk::DescriptorType::eUniformBuffer;
		descriptor_write.descriptorCount = 1;
		descriptor_write.pBufferInfo = &buffer_info;

		device.updateDescriptorSets(descriptor_write, {});
	}

}

void Renderer::createGraphicsPipeline()
{
    auto vert_shader_code = readFile("shaders/shader_vert.spv");
    auto frag_shader_code = readFile("shaders/shader_frag.spv");

    auto vert_shader_module = createShaderModule(vert_shader_code);
    auto frag_shader_module = createShaderModule(frag_shader_code);

    vk::PipelineShaderStageCreateInfo vert_stage_info;
    vert_stage_info.stage = vk::ShaderStageFlagBits::eVertex;
    vert_stage_info.module = vert_shader_module;
    vert_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo frag_stage_info;
    frag_stage_info.stage = vk::ShaderStageFlagBits::eFragment;
    frag_stage_info.module = frag_shader_module;
    frag_stage_info.pName = "main";

    vk::PipelineShaderStageCreateInfo shader_stages[] = { vert_stage_info,
                                                          frag_stage_info };

    vk::PipelineVertexInputStateCreateInfo vertex_input_info;
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr; // Optional
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr; // Optional

    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
    input_assembly.topology = vk::PrimitiveTopology::eTriangleList;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    vk::Viewport viewport;
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = (float)swap_chain_extent.width;
    viewport.height = (float)swap_chain_extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vk::Rect2D scissor;
    scissor.offset = { 0, 0 };
    scissor.extent = swap_chain_extent;

    vk::PipelineViewportStateCreateInfo viewport_state;
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = &viewport;
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = &scissor;

    vk::PipelineRasterizationStateCreateInfo rasterizer;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = vk::PolygonMode::eFill;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = vk::CullModeFlagBits::eBack;
    rasterizer.frontFace = vk::FrontFace::eClockwise;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f; // Optional
    rasterizer.depthBiasClamp = 0.0f;          // Optional
    rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

    vk::PipelineMultisampleStateCreateInfo multisampling;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    color_blend_attachment.colorWriteMask =
        vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = vk::BlendFactor::eOne;
    color_blend_attachment.dstColorBlendFactor = vk::BlendFactor::eZero;
    color_blend_attachment.colorBlendOp = vk::BlendOp::eAdd;
    color_blend_attachment.srcAlphaBlendFactor = vk::BlendFactor::eOne;
    color_blend_attachment.dstAlphaBlendFactor = vk::BlendFactor::eZero;
    color_blend_attachment.alphaBlendOp = vk::BlendOp::eAdd;

    vk::PipelineColorBlendStateCreateInfo color_blending;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = vk::LogicOp::eCopy;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;
    color_blending.blendConstants[1] = 0.0f;
    color_blending.blendConstants[2] = 0.0f;
    color_blending.blendConstants[3] = 0.0f;

    vk::PipelineLayoutCreateInfo pipeline_layout_info;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    pipeline_layout = device.createPipelineLayout(pipeline_layout_info);

    vk::GraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr; // Optional
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = nullptr; // Optional
    pipeline_info.layout = pipeline_layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0;
    //pipelineInfo.basePipelineHandle = ; // Optional
    pipeline_info.basePipelineIndex = -1; // Optional

    graphics_pipeline = device.createGraphicsPipeline({}, pipeline_info);

    device.destroyShaderModule(vert_shader_module);
    device.destroyShaderModule(frag_shader_module);
}

void Renderer::createFramebuffers()
{
    swap_chain_framebuffers.resize(swap_chain_image_views.size());
    for (size_t i = 0; i < swap_chain_image_views.size(); i++)
    {
        vk::ImageView attachments[] = { swap_chain_image_views[i] };

        vk::FramebufferCreateInfo framebuffer_info;
        framebuffer_info.renderPass = render_pass;
        framebuffer_info.attachmentCount = 1;
        framebuffer_info.pAttachments = attachments;
        framebuffer_info.width = swap_chain_extent.width;
        framebuffer_info.height = swap_chain_extent.height;
        framebuffer_info.layers = 1;

        swap_chain_framebuffers[i] = device.createFramebuffer(framebuffer_info);
    }
}

void Renderer::createCommandPool()
{
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physical_device);

    VkCommandPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queueFamilyIndices.graphics.value();
    pool_info.flags = 0; // Optional

    command_pool = device.createCommandPool(pool_info);
}

void Renderer::createCommandBuffers()
{
    vk::CommandBufferAllocateInfo alloc_info;
    alloc_info.commandPool = command_pool;
    alloc_info.level = vk::CommandBufferLevel::ePrimary;
    alloc_info.commandBufferCount = (uint32_t)swap_chain_framebuffers.size();

    command_buffers = device.allocateCommandBuffers(alloc_info);

    for (size_t i = 0; i < command_buffers.size(); i++)
    {
        auto& command_buffer = command_buffers[i];
        vk::CommandBufferBeginInfo begin_info;
        command_buffer.begin(begin_info);

        vk::RenderPassBeginInfo render_pass_info;
        render_pass_info.renderPass = render_pass;
        render_pass_info.framebuffer = swap_chain_framebuffers[i];
        render_pass_info.renderArea.offset = { 0, 0 };
        render_pass_info.renderArea.extent = swap_chain_extent;

        vk::ClearValue clear_color =
            vk::ClearColorValue(std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f });
        render_pass_info.clearValueCount = 1;
        render_pass_info.pClearValues = &clear_color;

        command_buffer.beginRenderPass(render_pass_info,
                                       vk::SubpassContents::eInline);
        command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics,
                                    graphics_pipeline);


		command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipeline_layout, 0, descriptor_sets[i], {});

        command_buffer.draw(3, 1, 0, 0);
        command_buffer.endRenderPass();

        command_buffer.end();
    }
}

void Renderer::createSyncObjects()
{
    vk::SemaphoreCreateInfo semaphore_info;

    vk::FenceCreateInfo fence_info;
    fence_info.flags = vk::FenceCreateFlagBits::eSignaled;

    images_in_flight.resize(swap_chain_images.size(), {});
    fences_used.resize(swap_chain_images.size(), false);

    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        image_available_semaphores.push_back(
            device.createSemaphore(semaphore_info));
        render_finished_semaphores.push_back(
            device.createSemaphore(semaphore_info));

        in_flight_fences.push_back(device.createFence(fence_info));
    }
}

void Renderer::updateUniformBuffer(uint32_t current_image)
{
    float speed = 1.0 * dt;
    float forward = 0;
    float right = 0;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) forward += speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) forward -= speed;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) right += speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) right -= speed;

    float rot_speed = 1.0 * dt;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) pitch += rot_speed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) pitch -= rot_speed;
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw += rot_speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) yaw -= rot_speed;

    glm::vec3 camera_dir = glm::quat(glm::vec3(0, yaw, 0)) *
                 glm::quat(glm::vec3(0, 0, pitch)) * glm::vec3(1,0,0);
	

    glm::vec3 side = glm::cross(glm::vec3(0, 1, 0), camera_dir);
    camera_pos += camera_dir * forward + side * right;

    glm::vec4 vectors[2] = { glm::vec4(camera_pos, 0),
                             glm::vec4(glm::normalize(camera_dir), 0) };

    void* data = device.mapMemory(
        uniform_buffers_memory[current_image], 0, 2 * sizeof(glm::vec4));
    memcpy(data, &vectors, 2 * sizeof(glm::vec4));
    device.unmapMemory(uniform_buffers_memory[current_image]);
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

vk::ShaderModule Renderer::createShaderModule(const std::vector<char>& code)
{
    vk::ShaderModuleCreateInfo create_info;
    create_info.codeSize = code.size();
    create_info.pCode = reinterpret_cast<const uint32_t*>(code.data());

    return device.createShaderModule(create_info);
}

void Renderer::createBuffer(vk::DeviceSize size,
                            vk::BufferUsageFlags usage,
                            vk::MemoryPropertyFlags properties,
                            vk::Buffer& buffer,
                            vk::DeviceMemory& bufferMemory)
{
    vk::BufferCreateInfo buffer_info = {};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = vk::SharingMode::eExclusive;

    buffer = device.createBuffer(buffer_info);

    auto mem_requirements = device.getBufferMemoryRequirements(buffer);

    vk::MemoryAllocateInfo allocInfo = {};
    allocInfo.allocationSize = mem_requirements.size;
    allocInfo.memoryTypeIndex =
        findMemoryType(mem_requirements.memoryTypeBits, properties);

    bufferMemory = device.allocateMemory(allocInfo);

    device.bindBufferMemory(buffer, bufferMemory, 0);
}

uint32_t Renderer::findMemoryType(uint32_t typeFilter,
                                  vk::MemoryPropertyFlags properties)
{
    auto mem_properties = physical_device.getMemoryProperties();

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) ==
                properties)
        {
            return i;
        }
    }

    THROW_RUNTIME_ERROR("Failed to find suitable memory type!");
}
