#pragma once

#pragma warning (push, 0)
#include <optional>
#include <set>
#include <vulkan/vulkan.hpp>
#pragma warning (pop)

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
	void createGraphicsPipeline();
	void createFramebuffers();
	void createCommandPool();
	void createCommandBuffers();
	void createSyncObjects();

    QueueFamilyIndices findQueueFamilies(vk::PhysicalDevice device);
    bool isDeviceSuitable(vk::PhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(vk::PhysicalDevice device);
    vk::Extent2D
        chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);
	vk::ShaderModule createShaderModule(const std::vector<char>& code);

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
	vk::PipelineLayout pipeline_layout;

	vk::Pipeline graphics_pipeline;

	vk::CommandPool command_pool;
	std::vector<vk::CommandBuffer> command_buffers;

    vk::Queue graphics_queue;
    vk::Queue present_queue;

};