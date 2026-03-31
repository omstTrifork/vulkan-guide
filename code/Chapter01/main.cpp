/*
 * Chapter 1 – The Basics: Creating a Vulkan 1.4 Instance
 *
 * Vulkan 1.4 equivalent of the OpenGL/FreeGLUT initialisation from
 * https://paroj.github.io/gltut/Basics/Introduction.html
 *
 * Build (macOS, requires Vulkan SDK and GLFW installed):
 *   clang++ -std=c++17 main.cpp \
 *       -I$VULKAN_SDK/include \
 *       -L$VULKAN_SDK/lib -lvulkan \
 *       $(pkg-config --cflags --libs glfw3) \
 *       -o chapter01
 */

#include <vulkan/vulkan.h>

// Include GLFW after vulkan.h. GLFW detects the already-defined VK_VERSION_*
// macros and exposes its Vulkan helper functions (glfwGetRequiredInstanceExtensions, etc.).
#include <GLFW/glfw3.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
static constexpr int  kWindowWidth  = 800;
static constexpr int  kWindowHeight = 600;
static constexpr char kAppName[]    = "Chapter 01 – The Basics";
static constexpr char kEngineName[] = "No Engine";

#ifdef NDEBUG
static constexpr bool kEnableValidation = false;
#else
static constexpr bool kEnableValidation = true;
#endif

static const std::vector<const char*> kValidationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Check that every requested validation layer is available on this system.
static bool checkValidationLayerSupport()
{
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> available(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, available.data());

    for (const char* name : kValidationLayers) {
        bool found = false;
        for (const auto& props : available) {
            if (strcmp(name, props.layerName) == 0) { found = true; break; }
        }
        if (!found) return false;
    }
    return true;
}

// Collect the instance extensions we need:
//   • GLFW-reported surface extensions (VK_KHR_surface + platform surface)
//   • VK_KHR_portability_enumeration  – required for MoltenVK on macOS
//   • VK_EXT_metal_surface            – macOS/iOS Metal back-end
static std::vector<const char*> getRequiredExtensions()
{
    uint32_t glfwCount = 0;
    const char** glfwExts = glfwGetRequiredInstanceExtensions(&glfwCount);
    std::vector<const char*> extensions(glfwExts, glfwExts + glfwCount);

    // macOS portability: MoltenVK is a portability-subset implementation.
    // Vulkan 1.4 requires explicit opt-in to enumerate such devices.
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    // Metal surface extension needed by MoltenVK when not already included
    // by GLFW (older GLFW versions on macOS may omit it).
    extensions.push_back("VK_EXT_metal_surface");

    return extensions;
}

// ---------------------------------------------------------------------------
// Application class
// ---------------------------------------------------------------------------
class Chapter01App
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
    GLFWwindow* window_   = nullptr;
    VkInstance  instance_ = VK_NULL_HANDLE;

    // ------------------------------------------------------------------
    // Window
    // ------------------------------------------------------------------
    void initWindow()
    {
        if (!glfwInit())
            throw std::runtime_error("Failed to initialise GLFW");

        // Tell GLFW not to create an OpenGL context – we want Vulkan.
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        // Disable resizing to keep this chapter focused on Instance creation.
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        window_ = glfwCreateWindow(kWindowWidth, kWindowHeight, kAppName,
                                   nullptr, nullptr);
        if (!window_)
            throw std::runtime_error("Failed to create GLFW window");
    }

    // ------------------------------------------------------------------
    // Vulkan Instance
    // ------------------------------------------------------------------
    void createInstance()
    {
        if (kEnableValidation && !checkValidationLayerSupport())
            throw std::runtime_error(
                "Validation layers requested but not available");

        // Application metadata (optional but useful for driver diagnostics).
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = kAppName;
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = kEngineName;
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        // Request Vulkan 1.4 – the minimum version for this guide.
        appInfo.apiVersion         = VK_API_VERSION_1_4;

        auto extensions = getRequiredExtensions();

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
        createInfo.ppEnabledExtensionNames = extensions.data();

        // *** macOS portability flag ***
        // Without this, vkCreateInstance returns VK_ERROR_INCOMPATIBLE_DRIVER
        // on systems running MoltenVK (all current macOS Vulkan deployments).
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

        if (kEnableValidation) {
            createInfo.enabledLayerCount   =
                static_cast<uint32_t>(kValidationLayers.size());
            createInfo.ppEnabledLayerNames = kValidationLayers.data();
        } else {
            createInfo.enabledLayerCount = 0;
        }

        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance_);
        if (result != VK_SUCCESS)
            throw std::runtime_error(
                "vkCreateInstance failed (VkResult = " +
                std::to_string(static_cast<int>(result)) + ")");

        std::cout << "[OK] Vulkan 1.4 instance created successfully.\n";

        // Print available extensions for diagnostics.
        uint32_t extCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount, nullptr);
        std::vector<VkExtensionProperties> availExts(extCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extCount,
                                               availExts.data());
        std::cout << "[INFO] " << extCount
                  << " instance extensions supported.\n";
    }

    void initVulkan()
    {
        createInstance();
    }

    // ------------------------------------------------------------------
    // Main loop
    // ------------------------------------------------------------------
    void mainLoop()
    {
        std::cout << "[INFO] Entering main loop. Close the window to exit.\n";
        while (!glfwWindowShouldClose(window_)) {
            glfwPollEvents();
        }
    }

    // ------------------------------------------------------------------
    // Cleanup
    // ------------------------------------------------------------------
    void cleanup()
    {
        if (instance_ != VK_NULL_HANDLE)
            vkDestroyInstance(instance_, nullptr);

        if (window_)
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
        Chapter01App app;
        app.run();
    } catch (const std::exception& e) {
        std::cerr << "[ERROR] " << e.what() << "\n";
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
