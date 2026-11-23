// vulkan_ui.hpp — renderer UI oparty o Vulkan (Windows + Android).
// Rysuje DrawList (prostokąty + tekst bitmapowy 5x7 jako prostokąty).
// Brak zależności poza Vulkan SDK/NDK i shaderami kompilowanymi przez CMake.
#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <vector>
#include "ui_draw.hpp"

class VulkanUi {
public:
    bool createInstance(bool enableValidation = false);
    VkInstance instance() const { return instance_; }
    // surface tworzona przez platformę (GLFW / Android), ten obiekt przejmuje jej własność.
    bool init(VkSurfaceKHR surface, uint32_t width, uint32_t height);
    void resize(uint32_t width, uint32_t height);
    void render(const DrawList& dl);
    void shutdown();
    bool ready() const { return device_ != VK_NULL_HANDLE; }

private:
    struct Vertex { float pos[2]; float color[4]; };
    bool createDevice();
    bool createSwapchain();
    void cleanupSwapchain();
    bool createRenderPass();
    bool createPipeline();
    bool createCommandBuffers();
    bool ensureBuffers(size_t vertexCount, size_t indexCount);
    uint32_t findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const;
    bool createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buf, VkDeviceMemory& mem, void** mapped);

    VkInstance instance_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkPhysicalDevice phys_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    uint32_t queueFamily_ = 0;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapFormat_ = VK_FORMAT_B8G8R8A8_UNORM;
    VkExtent2D extent_{};
    std::vector<VkImage> swapImages_;
    std::vector<VkImageView> swapViews_;
    std::vector<VkFramebuffer> framebuffers_;
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_ = VK_NULL_HANDLE;
    VkPipeline pipeline_ = VK_NULL_HANDLE;
    VkCommandPool cmdPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> cmdBuffers_;
    VkSemaphore imageAvail_ = VK_NULL_HANDLE;
    VkSemaphore renderDone_ = VK_NULL_HANDLE;
    VkFence fence_ = VK_NULL_HANDLE;

    VkBuffer vbo_ = VK_NULL_HANDLE; VkDeviceMemory vboMem_ = VK_NULL_HANDLE; void* vboMapped_ = nullptr; size_t vboCap_ = 0;
    VkBuffer ibo_ = VK_NULL_HANDLE; VkDeviceMemory iboMem_ = VK_NULL_HANDLE; void* iboMapped_ = nullptr; size_t iboCap_ = 0;
    uint32_t width_ = 0, height_ = 0;
    bool needResize_ = false;
};
