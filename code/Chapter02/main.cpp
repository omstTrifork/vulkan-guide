/*
 * Chapter 2 – Hello, Triangle!
 *
 * Complete Vulkan 1.4 equivalent of gltut Tutorial 01 "Following the Data":
 * opens a window and renders a single solid-white triangle using a vertex
 * buffer, a GLSL vertex shader, and a GLSL fragment shader compiled to SPIR-V.
 *
 * Build (macOS, requires Vulkan SDK and GLFW installed via Homebrew):
 *   # First compile the shaders:
 *   glslc vert.glsl -o vert.spv
 *   glslc frag.glsl -o frag.spv
 *
 *   # Then build the application:
 *   clang++ -std=c++17 main.cpp \
 *       -I$VULKAN_SDK/include \
 *       -L$VULKAN_SDK/lib -lvulkan \
 *       $(pkg-config --cflags --libs glfw3) \
 *       -o chapter02
 *   ./chapter02
 *
 * NOTE: The SPIR-V binaries (vert.spv, frag.spv) must be present in the
 *       working directory when the program is run. See vert.glsl and frag.glsl
 *       in this directory.
 */

#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr uint32_t kWidth  = 800;
static constexpr uint32_t kHeight = 600;
static constexpr char     kTitle[] = "Chapter 02 – Hello, Triangle!";

#ifdef NDEBUG
static constexpr bool kValidation = false;
#else
static constexpr bool kValidation = true;
#endif

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

static const std::vector<const char*> kDeviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

// ---------------------------------------------------------------------------
// Vertex data — identical to gltut Tutorial 01 vertexPositions[]
// Four-component clip-space positions (x, y, z, w).
// ---------------------------------------------------------------------------
static const float kVertexData[] = {
     0.0f,  0.5f, 0.0f, 1.0f,   // top
    -0.5f, -0.5f, 0.0f, 1.0f,   // bottom-left
     0.5f, -0.5f, 0.0f, 1.0f,   // bottom-right
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static std::vector<char> readFile(const std::string& path)
{
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    if (!f.is_open())
        throw std::runtime_error("Cannot open file: " + path);
    size_t size = static_cast<size_t>(f.tellg());
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), static_cast<std::streamsize>(size));
    return buf;
}

static bool checkValidationLayerSupport()
{
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> available(count);
    vkEnumerateInstanceLayerProperties(&count, available.data());
    for (const char* name : kValidationLayers) {
        bool found = false;
        for (const auto& p : available)
            if (strcmp(name, p.layerName) == 0) { found = true; break; }
        if (!found) return false;
    }
    return true;
}

static uint32_t findMemoryType(VkPhysicalDevice physDev,
                                uint32_t typeFilter,
                                VkMemoryPropertyFlags props)
{
    VkPhysicalDeviceMemoryProperties memProps;
    vkGetPhysicalDeviceMemoryProperties(physDev, &memProps);
    for (uint32_t i = 0; i < memProps.memoryTypeCount; ++i) {
        if ((typeFilter & (1u << i)) &&
            (memProps.memoryTypes[i].propertyFlags & props) == props)
            return i;
    }
    throw std::runtime_error("No suitable memory type found");
}

// ---------------------------------------------------------------------------
// Application class
// ---------------------------------------------------------------------------
class Chapter02App
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    // ------ GLFW
    GLFWwindow* window_   = nullptr;

    // ------ Vulkan core objects
    VkInstance       instance_       = VK_NULL_HANDLE;
    VkSurfaceKHR     surface_        = VK_NULL_HANDLE;
    VkPhysicalDevice physDevice_     = VK_NULL_HANDLE;
    VkDevice         device_         = VK_NULL_HANDLE;
    VkQueue          graphicsQueue_  = VK_NULL_HANDLE;
    VkQueue          presentQueue_   = VK_NULL_HANDLE;

    // ------ Swap chain
    VkSwapchainKHR           swapChain_      = VK_NULL_HANDLE;
    std::vector<VkImage>     swapImages_;
    VkFormat                 swapFormat_     = VK_FORMAT_UNDEFINED;
    VkExtent2D               swapExtent_     = {};
    std::vector<VkImageView> swapImageViews_;

    // ------ Render pass + pipeline
    VkRenderPass     renderPass_      = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout_  = VK_NULL_HANDLE;
    VkPipeline       pipeline_        = VK_NULL_HANDLE;

    // ------ Framebuffers
    std::vector<VkFramebuffer> framebuffers_;

    // ------ Vertex buffer
    VkBuffer       vertexBuffer_       = VK_NULL_HANDLE;
    VkDeviceMemory vertexBufferMemory_ = VK_NULL_HANDLE;

    // ------ Commands
    VkCommandPool                commandPool_ = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers_;

    // ------ Synchronisation
    VkSemaphore imageAvailableSem_ = VK_NULL_HANDLE;
    VkSemaphore renderFinishedSem_ = VK_NULL_HANDLE;
    VkFence     inFlightFence_     = VK_NULL_HANDLE;

    // ------ Queue family indices
    uint32_t graphicsFamily_ = UINT32_MAX;
    uint32_t presentFamily_  = UINT32_MAX;

    // =================================================================
    // Window
    // =================================================================
    void initWindow()
    {
        if (!glfwInit())
            throw std::runtime_error("glfwInit failed");
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        window_ = glfwCreateWindow(kWidth, kHeight, kTitle, nullptr, nullptr);
        if (!window_) throw std::runtime_error("glfwCreateWindow failed");
    }

    // =================================================================
    // Vulkan initialisation
    // =================================================================
    void initVulkan()
    {
        createInstance();
        createSurface();
        pickPhysicalDevice();
        createLogicalDevice();
        createSwapChain();
        createImageViews();
        createRenderPass();
        createGraphicsPipeline();
        createFramebuffers();
        createVertexBuffer();
        createCommandPool();
        createCommandBuffers();
        createSyncObjects();
    }

    // -----------------------------------------------------------------
    // 1. Instance (same as Chapter 1)
    // -----------------------------------------------------------------
    void createInstance()
    {
        if (kValidation && !checkValidationLayerSupport())
            throw std::runtime_error("Validation layers not available");

        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = kTitle;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "No Engine";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_4;

        uint32_t glfwCount = 0;
        const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwCount);
        std::vector<const char*> extensions(glfwExts, glfwExts + glfwCount);
        extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        extensions.push_back("VK_EXT_metal_surface");

        VkInstanceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        ci.pApplicationInfo        = &appInfo;
        ci.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        ci.ppEnabledExtensionNames = extensions.data();
        ci.flags                  |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        if (kValidation) {
            ci.enabledLayerCount   = static_cast<uint32_t>(kValidationLayers.size());
            ci.ppEnabledLayerNames = kValidationLayers.data();
        }

        if (vkCreateInstance(&ci, nullptr, &instance_) != VK_SUCCESS)
            throw std::runtime_error("vkCreateInstance failed");
    }

    // -----------------------------------------------------------------
    // 2. Window surface — connects the Instance to the GLFW window
    // -----------------------------------------------------------------
    void createSurface()
    {
        if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_)
                != VK_SUCCESS)
            throw std::runtime_error("glfwCreateWindowSurface failed");
    }

    // -----------------------------------------------------------------
    // 3. Physical device selection
    // -----------------------------------------------------------------
    void pickPhysicalDevice()
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance_, &count, nullptr);
        if (count == 0) throw std::runtime_error("No Vulkan GPUs found");

        std::vector<VkPhysicalDevice> devices(count);
        vkEnumeratePhysicalDevices(instance_, &count, devices.data());

        for (auto dev : devices) {
            if (isDeviceSuitable(dev)) {
                physDevice_ = dev;
                break;
            }
        }
        if (physDevice_ == VK_NULL_HANDLE)
            throw std::runtime_error("No suitable GPU found");

        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(physDevice_, &props);
        std::cout << "[OK] GPU: " << props.deviceName << "\n";
    }

    bool isDeviceSuitable(VkPhysicalDevice dev)
    {
        findQueueFamilies(dev);
        if (graphicsFamily_ == UINT32_MAX || presentFamily_ == UINT32_MAX)
            return false;

        // Check that VK_KHR_swapchain is available.
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> exts(extCount);
        vkEnumerateDeviceExtensionProperties(dev, nullptr, &extCount,
                                             exts.data());
        for (const char* required : kDeviceExtensions) {
            bool found = false;
            for (const auto& e : exts)
                if (strcmp(required, e.extensionName) == 0) { found = true; break; }
            if (!found) return false;
        }

        // Check the swap chain has at least one format and present mode.
        uint32_t formatCount = 0, modeCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(dev, surface_, &formatCount, nullptr);
        vkGetPhysicalDeviceSurfacePresentModesKHR(dev, surface_, &modeCount, nullptr);
        return formatCount > 0 && modeCount > 0;
    }

    void findQueueFamilies(VkPhysicalDevice dev)
    {
        graphicsFamily_ = UINT32_MAX;
        presentFamily_  = UINT32_MAX;

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, nullptr);
        std::vector<VkQueueFamilyProperties> families(count);
        vkGetPhysicalDeviceQueueFamilyProperties(dev, &count, families.data());

        for (uint32_t i = 0; i < count; ++i) {
            if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                graphicsFamily_ = i;

            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface_,
                                                 &presentSupport);
            if (presentSupport) presentFamily_ = i;

            if (graphicsFamily_ != UINT32_MAX && presentFamily_ != UINT32_MAX)
                break;
        }
    }

    // -----------------------------------------------------------------
    // 4. Logical device + queues
    // -----------------------------------------------------------------
    void createLogicalDevice()
    {
        float priority = 1.0f;
        std::set<uint32_t> uniqueFamilies = {graphicsFamily_, presentFamily_};
        std::vector<VkDeviceQueueCreateInfo> queueInfos;
        for (uint32_t family : uniqueFamilies) {
            VkDeviceQueueCreateInfo qi{};
            qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            qi.queueFamilyIndex = family;
            qi.queueCount       = 1;
            qi.pQueuePriorities = &priority;
            queueInfos.push_back(qi);
        }

        // MoltenVK requires VK_KHR_portability_subset on the device as well.
        std::vector<const char*> exts(kDeviceExtensions.begin(),
                                      kDeviceExtensions.end());
        uint32_t extCount = 0;
        vkEnumerateDeviceExtensionProperties(physDevice_, nullptr,
                                             &extCount, nullptr);
        std::vector<VkExtensionProperties> available(extCount);
        vkEnumerateDeviceExtensionProperties(physDevice_, nullptr,
                                             &extCount, available.data());
        for (const auto& e : available)
            if (strcmp(e.extensionName, "VK_KHR_portability_subset") == 0) {
                exts.push_back("VK_KHR_portability_subset");
                break;
            }

        VkDeviceCreateInfo ci{};
        ci.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        ci.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
        ci.pQueueCreateInfos       = queueInfos.data();
        ci.enabledExtensionCount   = static_cast<uint32_t>(exts.size());
        ci.ppEnabledExtensionNames = exts.data();

        if (vkCreateDevice(physDevice_, &ci, nullptr, &device_) != VK_SUCCESS)
            throw std::runtime_error("vkCreateDevice failed");

        vkGetDeviceQueue(device_, graphicsFamily_, 0, &graphicsQueue_);
        vkGetDeviceQueue(device_, presentFamily_,  0, &presentQueue_);
    }

    // -----------------------------------------------------------------
    // 5. Swap chain
    // -----------------------------------------------------------------
    void createSwapChain()
    {
        VkSurfaceCapabilitiesKHR caps;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physDevice_, surface_, &caps);

        // Choose format: prefer VK_FORMAT_B8G8R8A8_SRGB.
        uint32_t fmtCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_,
                                             &fmtCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(fmtCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physDevice_, surface_,
                                             &fmtCount, formats.data());
        VkSurfaceFormatKHR chosen = formats[0];
        for (const auto& f : formats)
            if (f.format == VK_FORMAT_B8G8R8A8_SRGB &&
                f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            { chosen = f; break; }

        // Choose present mode: prefer FIFO (vsync, always available).
        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;

        // Choose extent.
        VkExtent2D extent = caps.currentExtent;
        if (caps.currentExtent.width == UINT32_MAX) {
            extent.width  = std::clamp(kWidth,
                                       caps.minImageExtent.width,
                                       caps.maxImageExtent.width);
            extent.height = std::clamp(kHeight,
                                       caps.minImageExtent.height,
                                       caps.maxImageExtent.height);
        }

        uint32_t imageCount = caps.minImageCount + 1;
        if (caps.maxImageCount > 0)
            imageCount = std::min(imageCount, caps.maxImageCount);

        VkSwapchainCreateInfoKHR ci{};
        ci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        ci.surface          = surface_;
        ci.minImageCount    = imageCount;
        ci.imageFormat      = chosen.format;
        ci.imageColorSpace  = chosen.colorSpace;
        ci.imageExtent      = extent;
        ci.imageArrayLayers = 1;
        ci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        ci.preTransform     = caps.currentTransform;
        ci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        ci.presentMode      = presentMode;
        ci.clipped          = VK_TRUE;

        uint32_t families[] = {graphicsFamily_, presentFamily_};
        if (graphicsFamily_ != presentFamily_) {
            ci.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            ci.queueFamilyIndexCount = 2;
            ci.pQueueFamilyIndices   = families;
        } else {
            ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }

        if (vkCreateSwapchainKHR(device_, &ci, nullptr, &swapChain_)
                != VK_SUCCESS)
            throw std::runtime_error("vkCreateSwapchainKHR failed");

        uint32_t imgCount = 0;
        vkGetSwapchainImagesKHR(device_, swapChain_, &imgCount, nullptr);
        swapImages_.resize(imgCount);
        vkGetSwapchainImagesKHR(device_, swapChain_, &imgCount,
                                swapImages_.data());

        swapFormat_ = chosen.format;
        swapExtent_ = extent;
    }

    // -----------------------------------------------------------------
    // 6. Image views
    // -----------------------------------------------------------------
    void createImageViews()
    {
        swapImageViews_.resize(swapImages_.size());
        for (size_t i = 0; i < swapImages_.size(); ++i) {
            VkImageViewCreateInfo ci{};
            ci.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ci.image    = swapImages_[i];
            ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ci.format   = swapFormat_;
            ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            ci.subresourceRange.baseMipLevel   = 0;
            ci.subresourceRange.levelCount     = 1;
            ci.subresourceRange.baseArrayLayer = 0;
            ci.subresourceRange.layerCount     = 1;
            if (vkCreateImageView(device_, &ci, nullptr, &swapImageViews_[i])
                    != VK_SUCCESS)
                throw std::runtime_error("vkCreateImageView failed");
        }
    }

    // -----------------------------------------------------------------
    // 7. Render pass
    // -----------------------------------------------------------------
    void createRenderPass()
    {
        VkAttachmentDescription color{};
        color.format         = swapFormat_;
        color.samples        = VK_SAMPLE_COUNT_1_BIT;
        color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        color.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorRef{};
        colorRef.attachment = 0;
        colorRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorRef;

        VkSubpassDependency dep{};
        dep.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dep.dstSubpass    = 0;
        dep.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.srcAccessMask = 0;
        dep.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dep.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo ci{};
        ci.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        ci.attachmentCount = 1;
        ci.pAttachments    = &color;
        ci.subpassCount    = 1;
        ci.pSubpasses      = &subpass;
        ci.dependencyCount = 1;
        ci.pDependencies   = &dep;

        if (vkCreateRenderPass(device_, &ci, nullptr, &renderPass_)
                != VK_SUCCESS)
            throw std::runtime_error("vkCreateRenderPass failed");
    }

    // -----------------------------------------------------------------
    // 8. Graphics pipeline
    // -----------------------------------------------------------------
    VkShaderModule createShaderModule(const std::vector<char>& code)
    {
        VkShaderModuleCreateInfo ci{};
        ci.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        ci.codeSize = code.size();
        ci.pCode    = reinterpret_cast<const uint32_t*>(code.data());
        VkShaderModule mod;
        if (vkCreateShaderModule(device_, &ci, nullptr, &mod) != VK_SUCCESS)
            throw std::runtime_error("vkCreateShaderModule failed");
        return mod;
    }

    void createGraphicsPipeline()
    {
        auto vertCode = readFile("vert.spv");
        auto fragCode = readFile("frag.spv");
        VkShaderModule vertMod = createShaderModule(vertCode);
        VkShaderModule fragMod = createShaderModule(fragCode);

        VkPipelineShaderStageCreateInfo stages[2] = {};
        stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        stages[0].module = vertMod;
        stages[0].pName  = "main";
        stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        stages[1].module = fragMod;
        stages[1].pName  = "main";

        // Vertex input — one binding, one vec4 attribute at location 0.
        // Matches glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0).
        VkVertexInputBindingDescription binding{};
        binding.binding   = 0;
        binding.stride    = 4 * sizeof(float);
        binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        VkVertexInputAttributeDescription attr{};
        attr.binding  = 0;
        attr.location = 0;
        attr.format   = VK_FORMAT_R32G32B32A32_SFLOAT;
        attr.offset   = 0;

        VkPipelineVertexInputStateCreateInfo vertexInput{};
        vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInput.vertexBindingDescriptionCount   = 1;
        vertexInput.pVertexBindingDescriptions      = &binding;
        vertexInput.vertexAttributeDescriptionCount = 1;
        vertexInput.pVertexAttributeDescriptions    = &attr;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

        VkViewport viewport{};
        viewport.width    = static_cast<float>(swapExtent_.width);
        viewport.height   = static_cast<float>(swapExtent_.height);
        viewport.maxDepth = 1.0f;
        VkRect2D scissor{{0, 0}, swapExtent_};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports    = &viewport;
        viewportState.scissorCount  = 1;
        viewportState.pScissors     = &scissor;

        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode    = VK_CULL_MODE_BACK_BIT;
        raster.frontFace   = VK_FRONT_FACE_CLOCKWISE;
        raster.lineWidth   = 1.0f;

        VkPipelineMultisampleStateCreateInfo ms{};
        ms.sType               = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState blendAtt{};
        blendAtt.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                  VK_COLOR_COMPONENT_G_BIT |
                                  VK_COLOR_COMPONENT_B_BIT |
                                  VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments    = &blendAtt;

        VkPipelineLayoutCreateInfo layoutCI{};
        layoutCI.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        if (vkCreatePipelineLayout(device_, &layoutCI, nullptr,
                                   &pipelineLayout_) != VK_SUCCESS)
            throw std::runtime_error("vkCreatePipelineLayout failed");

        VkGraphicsPipelineCreateInfo pipelineCI{};
        pipelineCI.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCI.stageCount          = 2;
        pipelineCI.pStages             = stages;
        pipelineCI.pVertexInputState   = &vertexInput;
        pipelineCI.pInputAssemblyState = &inputAssembly;
        pipelineCI.pViewportState      = &viewportState;
        pipelineCI.pRasterizationState = &raster;
        pipelineCI.pMultisampleState   = &ms;
        pipelineCI.pColorBlendState    = &blend;
        pipelineCI.layout              = pipelineLayout_;
        pipelineCI.renderPass          = renderPass_;
        pipelineCI.subpass             = 0;

        if (vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1,
                                      &pipelineCI, nullptr, &pipeline_)
                != VK_SUCCESS)
            throw std::runtime_error("vkCreateGraphicsPipelines failed");

        vkDestroyShaderModule(device_, fragMod, nullptr);
        vkDestroyShaderModule(device_, vertMod, nullptr);
    }

    // -----------------------------------------------------------------
    // 9. Framebuffers
    // -----------------------------------------------------------------
    void createFramebuffers()
    {
        framebuffers_.resize(swapImageViews_.size());
        for (size_t i = 0; i < swapImageViews_.size(); ++i) {
            VkFramebufferCreateInfo ci{};
            ci.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            ci.renderPass      = renderPass_;
            ci.attachmentCount = 1;
            ci.pAttachments    = &swapImageViews_[i];
            ci.width           = swapExtent_.width;
            ci.height          = swapExtent_.height;
            ci.layers          = 1;
            if (vkCreateFramebuffer(device_, &ci, nullptr, &framebuffers_[i])
                    != VK_SUCCESS)
                throw std::runtime_error("vkCreateFramebuffer failed");
        }
    }

    // -----------------------------------------------------------------
    // 10. Vertex buffer
    // -----------------------------------------------------------------
    void createVertexBuffer()
    {
        VkDeviceSize size = sizeof(kVertexData);

        VkBufferCreateInfo ci{};
        ci.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        ci.size        = size;
        ci.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device_, &ci, nullptr, &vertexBuffer_) != VK_SUCCESS)
            throw std::runtime_error("vkCreateBuffer failed");

        VkMemoryRequirements memReq;
        vkGetBufferMemoryRequirements(device_, vertexBuffer_, &memReq);

        VkMemoryAllocateInfo ai{};
        ai.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        ai.allocationSize  = memReq.size;
        ai.memoryTypeIndex = findMemoryType(
            physDevice_, memReq.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device_, &ai, nullptr, &vertexBufferMemory_)
                != VK_SUCCESS)
            throw std::runtime_error("vkAllocateMemory failed");

        vkBindBufferMemory(device_, vertexBuffer_, vertexBufferMemory_, 0);

        void* data;
        vkMapMemory(device_, vertexBufferMemory_, 0, size, 0, &data);
        memcpy(data, kVertexData, static_cast<size_t>(size));
        vkUnmapMemory(device_, vertexBufferMemory_);
    }

    // -----------------------------------------------------------------
    // 11. Command pool and command buffers
    // -----------------------------------------------------------------
    void createCommandPool()
    {
        VkCommandPoolCreateInfo ci{};
        ci.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        ci.queueFamilyIndex = graphicsFamily_;
        ci.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        if (vkCreateCommandPool(device_, &ci, nullptr, &commandPool_)
                != VK_SUCCESS)
            throw std::runtime_error("vkCreateCommandPool failed");
    }

    void createCommandBuffers()
    {
        commandBuffers_.resize(framebuffers_.size());
        VkCommandBufferAllocateInfo ai{};
        ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        ai.commandPool        = commandPool_;
        ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = static_cast<uint32_t>(commandBuffers_.size());
        if (vkAllocateCommandBuffers(device_, &ai, commandBuffers_.data())
                != VK_SUCCESS)
            throw std::runtime_error("vkAllocateCommandBuffers failed");

        recordCommandBuffers();
    }

    void recordCommandBuffers()
    {
        for (size_t i = 0; i < commandBuffers_.size(); ++i) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vkBeginCommandBuffer(commandBuffers_[i], &beginInfo);

            VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
            VkRenderPassBeginInfo rpBegin{};
            rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            rpBegin.renderPass        = renderPass_;
            rpBegin.framebuffer       = framebuffers_[i];
            rpBegin.renderArea.extent = swapExtent_;
            rpBegin.clearValueCount   = 1;
            rpBegin.pClearValues      = &clearColor;

            vkCmdBeginRenderPass(commandBuffers_[i], &rpBegin,
                                 VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(commandBuffers_[i],
                              VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_);

            VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commandBuffers_[i], 0, 1,
                                   &vertexBuffer_, &offset);

            // Draw call — glDrawArrays(GL_TRIANGLES, 0, 3) equivalent
            vkCmdDraw(commandBuffers_[i], 3, 1, 0, 0);

            vkCmdEndRenderPass(commandBuffers_[i]);
            if (vkEndCommandBuffer(commandBuffers_[i]) != VK_SUCCESS)
                throw std::runtime_error("vkEndCommandBuffer failed");
        }
    }

    // -----------------------------------------------------------------
    // 12. Synchronisation objects
    // -----------------------------------------------------------------
    void createSyncObjects()
    {
        VkSemaphoreCreateInfo semCI{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        VkFenceCreateInfo fenceCI{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                                  nullptr,
                                  VK_FENCE_CREATE_SIGNALED_BIT};
        if (vkCreateSemaphore(device_, &semCI, nullptr, &imageAvailableSem_)
                != VK_SUCCESS ||
            vkCreateSemaphore(device_, &semCI, nullptr, &renderFinishedSem_)
                != VK_SUCCESS ||
            vkCreateFence(device_, &fenceCI, nullptr, &inFlightFence_)
                != VK_SUCCESS)
            throw std::runtime_error("Failed to create sync objects");
    }

    // =================================================================
    // Main loop
    // =================================================================
    void mainLoop()
    {
        std::cout << "[INFO] Rendering Hello Triangle. Close the window to exit.\n";
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
            drawFrame();
        }
        vkDeviceWaitIdle(device_);
    }

    void drawFrame()
    {
        // Wait for the GPU to finish the previous frame.
        vkWaitForFences(device_, 1, &inFlightFence_, VK_TRUE, UINT64_MAX);
        vkResetFences(device_, 1, &inFlightFence_);

        // Acquire an image from the swap chain.
        uint32_t imageIndex;
        vkAcquireNextImageKHR(device_, swapChain_, UINT64_MAX,
                              imageAvailableSem_, VK_NULL_HANDLE, &imageIndex);

        // Submit the command buffer for that image.
        VkPipelineStageFlags waitStage =
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount   = 1;
        submit.pWaitSemaphores      = &imageAvailableSem_;
        submit.pWaitDstStageMask    = &waitStage;
        submit.commandBufferCount   = 1;
        submit.pCommandBuffers      = &commandBuffers_[imageIndex];
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores    = &renderFinishedSem_;
        if (vkQueueSubmit(graphicsQueue_, 1, &submit, inFlightFence_)
                != VK_SUCCESS)
            throw std::runtime_error("vkQueueSubmit failed");

        // Present — glutSwapBuffers() equivalent.
        VkPresentInfoKHR present{};
        present.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores    = &renderFinishedSem_;
        present.swapchainCount     = 1;
        present.pSwapchains        = &swapChain_;
        present.pImageIndices      = &imageIndex;
        vkQueuePresentKHR(presentQueue_, &present);
    }

    // =================================================================
    // Cleanup — every object destroyed in reverse creation order
    // =================================================================
    void cleanup()
    {
        vkDestroyFence(device_,     inFlightFence_,     nullptr);
        vkDestroySemaphore(device_, renderFinishedSem_, nullptr);
        vkDestroySemaphore(device_, imageAvailableSem_, nullptr);

        vkFreeMemory(device_, vertexBufferMemory_, nullptr);
        vkDestroyBuffer(device_, vertexBuffer_, nullptr);

        for (auto fb : framebuffers_)
            vkDestroyFramebuffer(device_, fb, nullptr);

        vkDestroyPipeline(device_, pipeline_, nullptr);
        vkDestroyPipelineLayout(device_, pipelineLayout_, nullptr);
        vkDestroyRenderPass(device_, renderPass_, nullptr);

        for (auto iv : swapImageViews_)
            vkDestroyImageView(device_, iv, nullptr);

        vkDestroySwapchainKHR(device_, swapChain_, nullptr);
        vkDestroyCommandPool(device_, commandPool_, nullptr);

        vkDestroyDevice(device_, nullptr);
        vkDestroySurfaceKHR(instance_, surface_, nullptr);
        vkDestroyInstance(instance_, nullptr);

        glfwDestroyWindow(window_);
        glfwTerminate();
    }
};

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------
int main()
{
    try {
        Chapter02App app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
