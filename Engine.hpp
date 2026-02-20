#pragma once
#include "config.hpp"
#include "common.hpp"

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;
    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value() && transferFamily.has_value();
    }
};
struct SurfaceDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> modes;
};
struct Vertex {
    float vx, vy, vz;
    float nx, ny, nz;
    float u, v;
};
struct PushConstants {
    VkDeviceAddress vertexBufferAddress;
};

struct Engine {
    Engine();
    ~Engine();
    void run();
    void recordCmdBuffer(VkCommandBuffer& cmdBuffer, uint32_t imageIndex);

    void createWindow();
    void createInstance();
    void createSurface();
    void createDevice();
    void createSwapchain();
    void recreateSwapchain();
    void cleanupSwapchain();
    void createImageView(VkImage& image, VkImageView& imageView, VkImageAspectFlags aspectMask, VkFormat format);
    void createGfxPipelineLayout();
    void createGfxPipeline();
    void createShaderModule(std::vector<char> code, VkShaderModule& shaderModule);
    void createCommandPool(VkCommandPool& cmdPool, uint32_t queueFamilyIndex);
    void createSemaphore(VkSemaphore& sem);
    void createFence(VkFence& fence, VkFenceCreateFlags flags);
    void createBuffer(VkBuffer& buffer, VkDeviceMemory& bufferMemory, VkDeviceSize size, VkBufferUsageFlags usage, 
        VkMemoryPropertyFlags memProperties);
    void createVertexBuffer();
    void createIndexBuffer();

    GLFWwindow* window;
    VkInstance instance;
    VkSurfaceKHR surface;
    VkPhysicalDevice pDevice;
    VkDevice device;
    QueueFamilyIndices queueFamilyIndices;
    VkQueue gfxQueue;
    VkQueue presentQueue;
    VkQueue transferQueue;
    VkSwapchainKHR swapchain;
    VkExtent2D swapchainExtent;
    VkFormat swapchainFormat;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkPipeline gfxPipeline;
    VkPipelineLayout gfxPipelineLayout;
    VkCommandPool gfxCmdPool;
    VkCommandPool presentCmdPool;
    VkCommandPool transferCmdPool;
    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;
    VkDeviceSize vertexBufferSize;
    VkDeviceAddress vertexBufferAddress;
    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;
    VkDeviceSize indexBufferSize;
    PushConstants pushConstants;

    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;
    const uint32_t MAX_FRAMES_IN_FLIGHT = 4;
    uint32_t currFrame = 0;
    std::vector<const char*> instanceLayers = {
        "VK_LAYER_KHRONOS_validation"
    };
    std::vector<const char*> instanceExtensions = {
        "VK_KHR_surface",
        "VK_KHR_xcb_surface"
    };
    std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    const std::vector<Vertex> vertices = {
        {-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f},
        {0.5f, -0.5f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f},
        {0.5f, 0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f},
        {-0.5f, 0.5f, 0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f}
    };
    const std::vector<uint32_t> indices = {
        0, 1, 2, 2, 3, 0
    };

    bool checkInstanceLayersSupport();
    bool checkInstanceExtensionsSupport();
    bool checkDeviceExtensionsSupport(VkPhysicalDevice dev);
    bool isDeviceSuitable(VkPhysicalDevice dev);
    QueueFamilyIndices getQueueFamilyIndices(VkPhysicalDevice dev);
    SurfaceDetails getSurfaceDetails(VkPhysicalDevice dev);
    VkExtent2D chooseSurfaceExtent(VkSurfaceCapabilitiesKHR capabilities);
    VkPresentModeKHR choosePresentMode(std::vector<VkPresentModeKHR> modes);
    VkSurfaceFormatKHR chooseSurfaceFormat(std::vector<VkSurfaceFormatKHR> formats);
    VkCommandBuffer allocateCommandBuffer(VkCommandPool& cmdPool);
    uint32_t getMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags memProperties);
    void copyBuffer(VkBuffer& srcBuffer, VkBuffer& dstBuffer, VkDeviceSize size);
};