#pragma once

#include "util/timer.hpp"

#include <glm/glm.hpp>
#include <optional>
#include <set>
#include <vulkan/vulkan.hpp>

#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYERS = false;
#else
const bool ENABLE_VALIDATION_LAYERS = true;
#endif

struct GLFWwindow;

struct QueueFamilyIndices
{
    std::optional<uint32_t> graphics;
    std::optional<uint32_t> present;
    bool isComplete() { return graphics.has_value() && present.has_value(); }
    std::set<uint32_t> getUniqueFamilies()
    {
        return { graphics.value(), present.value() };
    }
};

struct SwapChainSupportDetails
{
    vk::SurfaceCapabilitiesKHR capabilities;
    std::vector<vk::SurfaceFormatKHR> formats;
    std::vector<vk::PresentModeKHR> present_modes;
};

class Renderer
{
  public:
    void init();
    void cleanup();

    void render();

    void updateWindow();

    bool isRunning();

  private:
    void initWindow();
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createDescriptorSetLayout();
    void createGraphicsPipeline();
    void createFramebuffers();
    void createCommandPool();
    void createUniformBuffers();
    void createDescriptorPool();
    void createDescriptorSets();
    void createCommandBuffers();
    void createSyncObjects();

    void updateUniformBuffer(uint32_t image_index);

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
    bool isDeviceSuitable(vk::PhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
    vk::Extent2D
        chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
    vk::ShaderModule createShaderModule(const std::vector<char>& code);
    void createBuffer(vk::DeviceSize size,
                      vk::BufferUsageFlags usage,
                      vk::MemoryPropertyFlags properties,
                      vk::Buffer& buffer,
                      vk::DeviceMemory& bufferMemory);
    uint32_t findMemoryType(uint32_t typeFilter,
                            vk::MemoryPropertyFlags properties);

    glm::vec3 camera_pos;
	float yaw = 0;
	float pitch = 0;

    Timer timer;
    float dt = 0.f;

    int fps_counter = 0;
    Timer fps_timer;

    uint32_t window_width = 0;
    uint32_t window_height = 0;

    const int MAX_FRAMES_IN_FLIGHT = 2;

    std::vector<VkSemaphore> image_available_semaphores;
    std::vector<VkSemaphore> render_finished_semaphores;
    std::vector<vk::Fence> in_flight_fences;
    std::vector<vk::Fence> images_in_flight;
    std::vector<bool> fences_used;
    size_t current_frame = 0;

    GLFWwindow* window = nullptr;

    vk::Instance vk_instance;
    vk::DispatchLoaderDynamic dldi;
    vk::DebugUtilsMessengerEXT debug_messenger;
    vk::PhysicalDevice physical_device;
    vk::Device device;
    vk::SurfaceKHR surface;
    vk::SwapchainKHR swap_chain;
    std::vector<vk::Image> swap_chain_images;
    std::vector<vk::ImageView> swap_chain_image_views;
    vk::Extent2D swap_chain_extent;
    vk::Format swap_chain_format;
    std::vector<vk::Framebuffer> swap_chain_framebuffers;

    vk::RenderPass render_pass;
    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout pipeline_layout;

    vk::DescriptorPool descriptor_pool;
    std::vector<vk::DescriptorSet> descriptor_sets;

    vk::Pipeline graphics_pipeline;

    std::vector<vk::Buffer> uniform_buffers;
    std::vector<vk::DeviceMemory> uniform_buffers_memory;

    vk::CommandPool command_pool;
    std::vector<vk::CommandBuffer> command_buffers;

    vk::Queue graphics_queue;
    vk::Queue present_queue;
};