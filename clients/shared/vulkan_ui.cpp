#include "vulkan_ui.hpp"
#include <cstring>
#include <algorithm>
#include <array>

// Nagłówki generowane przez CMake z clients/shaders/ui.vert|frag
// (glslangValidator -V --vn ui_vert_spv / ui_frag_spv z Vulkan SDK).
#include "ui_vert_spv.h"
#include "ui_frag_spv.h"

#define VK_CHECK(x) do { if ((x) != VK_SUCCESS) return false; } while (0)

bool VulkanUi::createInstance(bool enableValidation) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "AutoSerwis Pro";
    app.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app.pEngineName = "AutoSerwisUI";
    app.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app.apiVersion = VK_API_VERSION_1_0;

    std::vector<const char*> exts = { VK_KHR_SURFACE_EXTENSION_NAME };
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    exts.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_ANDROID_KHR)
    exts.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    exts.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
#elif defined(VK_USE_PLATFORM_XLIB_KHR)
    exts.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif
    // Wymagane rozszerzenia powierzchni zależą od platformy (define z CMake):
    // Windows: VK_KHR_win32_surface, Android: VK_KHR_android_surface, Linux: xlib/wayland.
    // GLFW (desktop) wymaga dokładnie tych samych rozszerzeń, więc lista powyżej wystarcza.

    std::vector<const char*> layers;
    if (enableValidation) layers.push_back("VK_LAYER_KHRONOS_validation");

    VkInstanceCreateInfo ci{};
    ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ci.pApplicationInfo = &app;
    ci.enabledExtensionCount = (uint32_t)exts.size();
    ci.ppEnabledExtensionNames = exts.data();
    ci.enabledLayerCount = (uint32_t)layers.size();
    ci.ppEnabledLayerNames = layers.empty() ? nullptr : layers.data();
    return vkCreateInstance(&ci, nullptr, &instance_) == VK_SUCCESS;
}

uint32_t VulkanUi::findMemoryType(uint32_t typeBits, VkMemoryPropertyFlags props) const {
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(phys_, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++)
        if ((typeBits & (1u << i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i;
    return 0;
}

bool VulkanUi::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer& buf, VkDeviceMemory& mem, void** mapped) {
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = size; bci.usage = usage; bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(device_, &bci, nullptr, &buf));
    VkMemoryRequirements mr{};
    vkGetBufferMemoryRequirements(device_, buf, &mr);
    VkMemoryAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    ai.allocationSize = mr.size;
    ai.memoryTypeIndex = findMemoryType(mr.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    VK_CHECK(vkAllocateMemory(device_, &ai, nullptr, &mem));
    VK_CHECK(vkBindBufferMemory(device_, buf, mem, 0));
    if (mapped) VK_CHECK(vkMapMemory(device_, mem, 0, size, 0, mapped));
    return true;
}

bool VulkanUi::createDevice() {
    uint32_t count = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, nullptr));
    if (count == 0) return false;
    std::vector<VkPhysicalDevice> devs(count);
    VK_CHECK(vkEnumeratePhysicalDevices(instance_, &count, devs.data()));
    // Wybierz pierwsze urządzenie z kolejką graphics+present.
    for (auto dev : devs) {
        uint32_t qcount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, nullptr);
        std::vector<VkQueueFamilyProperties> qprops(qcount);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &qcount, qprops.data());
        for (uint32_t i = 0; i < qcount; i++) {
            VkBool32 present = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_, &present);
            if ((qprops[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && present) {
                phys_ = dev; queueFamily_ = i;
                float prio = 1.0f;
                VkDeviceQueueCreateInfo qci{};
                qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                qci.queueFamilyIndex = i; qci.queueCount = 1; qci.pQueuePriorities = &prio;
                const char* dext = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
                VkDeviceCreateInfo dci{};
                dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
                dci.queueCreateInfoCount = 1; dci.pQueueCreateInfos = &qci;
                dci.enabledExtensionCount = 1; dci.ppEnabledExtensionNames = &dext;
                VK_CHECK(vkCreateDevice(phys_, &dci, nullptr, &device_));
                vkGetDeviceQueue(device_, i, 0, &queue_);
                return true;
            }
        }
    }
    return false;
}

bool VulkanUi::createSwapchain() {
    VkSurfaceCapabilitiesKHR caps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(phys_, surface_, &caps));
    uint32_t fcount = 0;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys_, surface_, &fcount, nullptr));
    std::vector<VkSurfaceFormatKHR> formats(fcount);
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(phys_, surface_, &fcount, formats.data()));
    VkSurfaceFormatKHR chosen = formats[0];
    for (auto& f : formats)
        if (f.format == VK_FORMAT_B8G8R8A8_UNORM || f.format == VK_FORMAT_R8G8B8A8_UNORM) { chosen = f; break; }
    swapFormat_ = chosen.format;
    extent_ = caps.currentExtent;
    if (extent_.width == 0xFFFFFFFFu) {
        extent_.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, width_));
        extent_.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, height_));
    }
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    VkSwapchainCreateInfoKHR sci{};
    sci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface = surface_;
    sci.minImageCount = imageCount;
    sci.imageFormat = chosen.format;
    sci.imageColorSpace = chosen.colorSpace;
    sci.imageExtent = extent_;
    sci.imageArrayLayers = 1;
    sci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    sci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sci.preTransform = caps.currentTransform;
    sci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    sci.clipped = VK_TRUE;
    VK_CHECK(vkCreateSwapchainKHR(device_, &sci, nullptr, &swapchain_));

    vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
    swapImages_.resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, swapImages_.data()));
    swapViews_.resize(imageCount);
    for (size_t i = 0; i < swapImages_.size(); i++) {
        VkImageViewCreateInfo vci{};
        vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        vci.image = swapImages_[i]; vci.viewType = VK_IMAGE_VIEW_TYPE_2D; vci.format = swapFormat_;
        vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vci.subresourceRange.levelCount = 1; vci.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device_, &vci, nullptr, &swapViews_[i]));
    }
    framebuffers_.resize(swapViews_.size());
    for (size_t i = 0; i < swapViews_.size(); i++) {
        VkFramebufferCreateInfo fci{};
        fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fci.renderPass = renderPass_; fci.attachmentCount = 1; fci.pAttachments = &swapViews_[i];
        fci.width = extent_.width; fci.height = extent_.height; fci.layers = 1;
        VK_CHECK(vkCreateFramebuffer(device_, &fci, nullptr, &framebuffers_[i]));
    }
    return true;
}

void VulkanUi::cleanupSwapchain() {
    for (auto fb : framebuffers_) if (fb) vkDestroyFramebuffer(device_, fb, nullptr);
    for (auto v : swapViews_) if (v) vkDestroyImageView(device_, v, nullptr);
    if (swapchain_) vkDestroySwapchainKHR(device_, swapchain_, nullptr);
    framebuffers_.clear(); swapViews_.clear(); swapImages_.clear(); swapchain_ = VK_NULL_HANDLE;
}

bool VulkanUi::createRenderPass() {
    VkAttachmentDescription ad{};
    ad.format = swapFormat_;
    ad.samples = VK_SAMPLE_COUNT_1_BIT;
    ad.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ad.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    ad.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ad.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference ref{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sub{};
    sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    sub.colorAttachmentCount = 1; sub.pColorAttachments = &ref;
    VkSubpassDependency dep{};
    dep.srcSubpass = VK_SUBPASS_EXTERNAL; dep.dstSubpass = 0;
    dep.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo rci{};
    rci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rci.attachmentCount = 1; rci.pAttachments = &ad;
    rci.subpassCount = 1; rci.pSubpasses = &sub;
    rci.dependencyCount = 1; rci.pDependencies = &dep;
    return vkCreateRenderPass(device_, &rci, nullptr, &renderPass_) == VK_SUCCESS;
}

bool VulkanUi::createPipeline() {
    auto makeShader = [&](const uint32_t* code, size_t sizeWords, VkShaderModule& out) {
        VkShaderModuleCreateInfo sci{};
        sci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        sci.codeSize = sizeWords * 4; sci.pCode = code;
        return vkCreateShaderModule(device_, &sci, nullptr, &out) == VK_SUCCESS;
    };
    VkShaderModule vert = VK_NULL_HANDLE, frag = VK_NULL_HANDLE;
    if (!makeShader(ui_vert_spv, sizeof(ui_vert_spv) / sizeof(uint32_t), vert)) return false;
    if (!makeShader(ui_frag_spv, sizeof(ui_frag_spv) / sizeof(uint32_t), frag)) return false;

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; stages[0].module = vert; stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; stages[1].module = frag; stages[1].pName = "main";

    VkVertexInputBindingDescription bind{0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription attrs[2]{};
    attrs[0].binding = 0; attrs[0].location = 0; attrs[0].format = VK_FORMAT_R32G32_SFLOAT; attrs[0].offset = offsetof(Vertex, pos);
    attrs[1].binding = 0; attrs[1].location = 1; attrs[1].format = VK_FORMAT_R32G32B32A32_SFLOAT; attrs[1].offset = offsetof(Vertex, color);
    VkPipelineVertexInputStateCreateInfo vin{};
    vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vin.vertexBindingDescriptionCount = 1; vin.pVertexBindingDescriptions = &bind;
    vin.vertexAttributeDescriptionCount = 2; vin.pVertexAttributeDescriptions = attrs;

    VkPipelineInputAssemblyStateCreateInfo ia{};
    ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp{};
    vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    vp.viewportCount = 1; vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rs{};
    rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE;
    rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.0f;

    VkPipelineMultisampleStateCreateInfo ms{};
    ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState blend{};
    blend.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    blend.blendEnable = VK_TRUE;
    blend.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA; blend.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.colorBlendOp = VK_BLEND_OP_ADD;
    blend.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; blend.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    blend.alphaBlendOp = VK_BLEND_OP_ADD;
    VkPipelineColorBlendStateCreateInfo cb{};
    cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    cb.attachmentCount = 1; cb.pAttachments = &blend;

    VkDynamicState dynStates[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{};
    dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dyn.dynamicStateCount = 2; dyn.pDynamicStates = dynStates;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; pcr.offset = 0; pcr.size = sizeof(float) * 2;
    VkPipelineLayoutCreateInfo lci{};
    lci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    lci.pushConstantRangeCount = 1; lci.pPushConstantRanges = &pcr;
    VK_CHECK(vkCreatePipelineLayout(device_, &lci, nullptr, &pipelineLayout_));

    VkGraphicsPipelineCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pci.stageCount = 2; pci.pStages = stages;
    pci.pVertexInputState = &vin; pci.pInputAssemblyState = &ia; pci.pViewportState = &vp;
    pci.pRasterizationState = &rs; pci.pMultisampleState = &ms; pci.pColorBlendState = &cb;
    pci.pDynamicState = &dyn; pci.layout = pipelineLayout_; pci.renderPass = renderPass_; pci.subpass = 0;
    bool ok = vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &pci, nullptr, &pipeline_) == VK_SUCCESS;
    vkDestroyShaderModule(device_, vert, nullptr);
    vkDestroyShaderModule(device_, frag, nullptr);
    return ok;
}

bool VulkanUi::createCommandBuffers() {
    VkCommandPoolCreateInfo pci{};
    pci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pci.queueFamilyIndex = queueFamily_;
    pci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    VK_CHECK(vkCreateCommandPool(device_, &pci, nullptr, &cmdPool_));
    cmdBuffers_.resize(framebuffers_.size());
    VkCommandBufferAllocateInfo ai{};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = cmdPool_; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandBufferCount = (uint32_t)cmdBuffers_.size();
    VK_CHECK(vkAllocateCommandBuffers(device_, &ai, cmdBuffers_.data()));
    VkSemaphoreCreateInfo sci{};
    sci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VK_CHECK(vkCreateSemaphore(device_, &sci, nullptr, &imageAvail_));
    VK_CHECK(vkCreateSemaphore(device_, &sci, nullptr, &renderDone_));
    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    VK_CHECK(vkCreateFence(device_, &fci, nullptr, &fence_));
    return true;
}

bool VulkanUi::init(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    surface_ = surface; width_ = width; height_ = height;
    if (!createDevice()) return false;
    if (!createRenderPass()) return false;
    if (!createSwapchain()) return false;
    if (!createPipeline()) return false;
    if (!createCommandBuffers()) return false;
    return true;
}

void VulkanUi::resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0) return;
    width_ = width; height_ = height; needResize_ = true;
}

bool VulkanUi::ensureBuffers(size_t vertexCount, size_t indexCount) {
    if (vertexCount > vboCap_) {
        if (vbo_) { vkUnmapMemory(device_, vboMem_); vkDestroyBuffer(device_, vbo_, nullptr); vkFreeMemory(device_, vboMem_, nullptr); vbo_ = VK_NULL_HANDLE; }
        size_t cap = std::max<size_t>(vertexCount, 65536);
        if (!createBuffer((VkDeviceSize)(cap * sizeof(Vertex)), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vbo_, vboMem_, &vboMapped_)) return false;
        vboCap_ = cap;
    }
    if (indexCount > iboCap_) {
        if (ibo_) { vkUnmapMemory(device_, iboMem_); vkDestroyBuffer(device_, ibo_, nullptr); vkFreeMemory(device_, iboMem_, nullptr); ibo_ = VK_NULL_HANDLE; }
        size_t cap = std::max<size_t>(indexCount, 98304);
        if (!createBuffer((VkDeviceSize)(cap * sizeof(uint32_t)), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, ibo_, iboMem_, &iboMapped_)) return false;
        iboCap_ = cap;
    }
    return true;
}

void VulkanUi::render(const DrawList& dl) {
    if (!ready() || extent_.width == 0 || extent_.height == 0) return;
    if (needResize_) {
        vkDeviceWaitIdle(device_);
        cleanupSwapchain();
        // framebuffery zależą od swapchain — komendy alokujemy od nowa tylko gdy zmieniła się ich liczba;
        // przy tej samej liczbie command buffery zostają ważne (renderPass bez zmian).
        if (!createSwapchain()) return;
        if (cmdBuffers_.size() != framebuffers_.size()) {
            vkFreeCommandBuffers(device_, cmdPool_, (uint32_t)cmdBuffers_.size(), cmdBuffers_.data());
            cmdBuffers_.resize(framebuffers_.size());
            VkCommandBufferAllocateInfo ai{};
            ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            ai.commandPool = cmdPool_; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            ai.commandBufferCount = (uint32_t)cmdBuffers_.size();
            if (vkAllocateCommandBuffers(device_, &ai, cmdBuffers_.data()) != VK_SUCCESS) return;
        }
        needResize_ = false;
    }

    // --- Złóż wierzchołki z DrawList ---
    std::vector<Vertex> verts;
    std::vector<uint32_t> indices;
    verts.reserve(dl.rects.size() * 4 + dl.texts.size() * 64);
    auto pushRect = [&](float x, float y, float w, float h, UiColor c) {
        uint32_t base = (uint32_t)verts.size();
        float col[4] = {c.r, c.g, c.b, c.a};
        Vertex v[4] = {{{x, y}, {col[0], col[1], col[2], col[3]}},
                       {{x + w, y}, {col[0], col[1], col[2], col[3]}},
                       {{x + w, y + h}, {col[0], col[1], col[2], col[3]}},
                       {{x, y + h}, {col[0], col[1], col[2], col[3]}}};
        verts.insert(verts.end(), v, v + 4);
        uint32_t idx[6] = {base, base + 1, base + 2, base, base + 2, base + 3};
        indices.insert(indices.end(), idx, idx + 6);
    };
    for (auto& r : dl.rects) pushRect(r.x, r.y, r.w, r.h, r.color);
    for (auto& t : dl.texts) {
        std::string s = uiSanitize(t.text);
        float cx = t.x;
        for (char ch : s) {
            const uint8_t* g = glyph5x7(ch);
            for (int row = 0; row < 7; row++) {
                uint8_t bits = g[row];
                for (int col = 0; col < 5; col++) {
                    if (bits & (1u << (4 - col)))
                        pushRect(cx + col * t.scale, t.y + row * t.scale, t.scale, t.scale, t.color);
                }
            }
            cx += 6 * t.scale;
            if (t.maxWidth > 0 && cx > t.x + t.maxWidth) break;
        }
    }
    if (verts.empty()) return;
    if (!ensureBuffers(verts.size(), indices.size())) return;
    memcpy(vboMapped_, verts.data(), verts.size() * sizeof(Vertex));
    memcpy(iboMapped_, indices.data(), indices.size() * sizeof(uint32_t));

    vkWaitForFences(device_, 1, &fence_, VK_TRUE, UINT64_MAX);
    uint32_t imageIndex = 0;
    VkResult acq = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvail_, VK_NULL_HANDLE, &imageIndex);
    if (acq == VK_ERROR_OUT_OF_DATE_KHR) { needResize_ = true; return; }
    if (acq != VK_SUCCESS && acq != VK_SUBOPTIMAL_KHR) return;
    vkResetFences(device_, 1, &fence_);

    VkCommandBuffer cmd = cmdBuffers_[imageIndex];
    vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(cmd, &begin) != VK_SUCCESS) return;

    VkClearValue clear{};
    clear.color.float32[0] = ui::BG.r; clear.color.float32[1] = ui::BG.g;
    clear.color.float32[2] = ui::BG.b; clear.color.float32[3] = 1.0f;
    VkRenderPassBeginInfo rp{};
    rp.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rp.renderPass = renderPass_; rp.framebuffer = framebuffers_[imageIndex];
    rp.renderArea.extent = extent_; rp.clearValueCount = 1; rp.pClearValues = &clear;
    vkCmdBeginRenderPass(cmd, &rp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport vp{};
    vp.width = (float)extent_.width; vp.height = (float)extent_.height; vp.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &vp);
    VkRect2D sc{}; sc.extent = extent_;
    vkCmdSetScissor(cmd, 0, 1, &sc);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);
    float screen[2] = {(float)extent_.width, (float)extent_.height};
    vkCmdPushConstants(cmd, pipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(screen), screen);
    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(cmd, 0, 1, &vbo_, &offset);
    vkCmdBindIndexBuffer(cmd, ibo_, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(cmd, (uint32_t)indices.size(), 1, 0, 0, 0);
    vkCmdEndRenderPass(cmd);
    if (vkEndCommandBuffer(cmd) != VK_SUCCESS) return;

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit.waitSemaphoreCount = 1; submit.pWaitSemaphores = &imageAvail_; submit.pWaitDstStageMask = &waitStage;
    submit.commandBufferCount = 1; submit.pCommandBuffers = &cmd;
    submit.signalSemaphoreCount = 1; submit.pSignalSemaphores = &renderDone_;
    if (vkQueueSubmit(queue_, 1, &submit, fence_) != VK_SUCCESS) return;

    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.waitSemaphoreCount = 1; present.pWaitSemaphores = &renderDone_;
    present.swapchainCount = 1; present.pSwapchains = &swapchain_; present.pImageIndices = &imageIndex;
    VkResult pres = vkQueuePresentKHR(queue_, &present);
    if (pres == VK_ERROR_OUT_OF_DATE_KHR || pres == VK_SUBOPTIMAL_KHR) needResize_ = true;
}

void VulkanUi::shutdown() {
    if (device_) {
        vkDeviceWaitIdle(device_);
        if (vbo_) { vkUnmapMemory(device_, vboMem_); vkDestroyBuffer(device_, vbo_, nullptr); vkFreeMemory(device_, vboMem_, nullptr); }
        if (ibo_) { vkUnmapMemory(device_, iboMem_); vkDestroyBuffer(device_, ibo_, nullptr); vkFreeMemory(device_, iboMem_, nullptr); }
        if (imageAvail_) vkDestroySemaphore(device_, imageAvail_, nullptr);
        if (renderDone_) vkDestroySemaphore(device_, renderDone_, nullptr);
        if (fence_) vkDestroyFence(device_, fence_, nullptr);
        if (cmdPool_) vkDestroyCommandPool(device_, cmdPool_, nullptr);
        if (pipeline_) vkDestroyPipeline(device_, pipeline_, nullptr);
        if (pipelineLayout_) vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        cleanupSwapchain();
        if (renderPass_) vkDestroyRenderPass(device_, renderPass_, nullptr);
        vkDestroyDevice(device_, nullptr);
        device_ = VK_NULL_HANDLE;
    }
    if (surface_) { vkDestroySurfaceKHR(instance_, surface_, nullptr); surface_ = VK_NULL_HANDLE; }
    if (instance_) { vkDestroyInstance(instance_, nullptr); instance_ = VK_NULL_HANDLE; }
}
