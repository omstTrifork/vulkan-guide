---
render_with_liquid: false
---
# Chapter 2: Hello, Triangle!

This chapter draws a single white triangle to the screen. The program defines three vertex positions on the CPU, uploads them to the GPU, writes a vertex shader and a fragment shader in GLSL, and issues a single draw call. This is the minimum viable Vulkan program.

Chapter 1 created the `VkInstance` — the very first step. This chapter adds the remaining eight steps and produces the triangle.

---

## Following the Data

The goal of this chapter is to understand how vertex data travels from a CPU array all the way to pixels on screen. Vulkan makes every step in that journey explicit.

```
CPU array  →  VkBuffer (vertex buffer)  →  vertex shader  →  fragment shader  →  VkImage (swap-chain)  →  screen
```

The pipeline in between contains:

1. **Input-assembler** — reads vertices from the vertex buffer according to the binding and attribute descriptions.
2. **Vertex shader** — transforms each vertex position into clip-space coordinates.
3. **Rasterizer** — converts triangles into fragments (pixels).
4. **Fragment shader** — assigns a colour to each fragment.
5. **Output-merger** (colour blend / depth test) — writes the fragment to the framebuffer attachment.

All of these stages are baked into a single immutable `VkPipeline` object at creation time.

---

## Step-by-Step: Building the Triangle

### Step 1 — Physical Device Selection

Vulkan lets you enumerate all available GPUs and choose the one you want.

```cpp
uint32_t deviceCount = 0;
vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
std::vector<VkPhysicalDevice> devices(deviceCount);
vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

// Pick the first discrete GPU, or fall back to integrated.
VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
for (auto dev : devices) {
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(dev, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        physicalDevice = dev;
        break;
    }
}
if (physicalDevice == VK_NULL_HANDLE)
    physicalDevice = devices[0]; // fallback
```

### Step 2 — Queue Families

Every GPU operation in Vulkan goes through a *queue*. Queues are grouped into *families*, each supporting a different set of operations (graphics, compute, transfer, present). You must find at least one family that supports graphics, and one that can present images to your surface.

```cpp
uint32_t queueFamilyCount = 0;
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                         &queueFamilyCount, nullptr);
std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice,
                                         &queueFamilyCount, families.data());

uint32_t graphicsFamily = UINT32_MAX, presentFamily = UINT32_MAX;
for (uint32_t i = 0; i < queueFamilyCount; ++i) {
    if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
        graphicsFamily = i;

    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface,
                                         &presentSupport);
    if (presentSupport) presentFamily = i;
}
```

### Step 3 — Logical Device and Queues

A `VkDevice` is your logical connection to the GPU. It is here that you request the queue families you found above.

```cpp
float queuePriority = 1.0f;
// Create one queue from each unique family.
std::vector<VkDeviceQueueCreateInfo> queueInfos;
for (uint32_t family : {graphicsFamily, presentFamily}) {
    VkDeviceQueueCreateInfo qi{};
    qi.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qi.queueFamilyIndex = family;
    qi.queueCount       = 1;
    qi.pQueuePriorities = &queuePriority;
    queueInfos.push_back(qi);
}

const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
VkDeviceCreateInfo deviceInfo{};
deviceInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
deviceInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueInfos.size());
deviceInfo.pQueueCreateInfos       = queueInfos.data();
deviceInfo.enabledExtensionCount   = 1;
deviceInfo.ppEnabledExtensionNames = deviceExtensions;

VkDevice device;
vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);

VkQueue graphicsQueue, presentQueue;
vkGetDeviceQueue(device, graphicsFamily, 0, &graphicsQueue);
vkGetDeviceQueue(device, presentFamily,  0, &presentQueue);
```

### Step 4 — Window Surface

A `VkSurfaceKHR` connects the Vulkan instance to the native window managed by GLFW.

```cpp
VkSurfaceKHR surface;
if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS)
    throw std::runtime_error("Failed to create window surface");
```

### Step 5 — Swap Chain

The swap chain is Vulkan's double-buffered display mechanism. You explicitly choose the surface format (e.g. `VK_FORMAT_B8G8R8A8_SRGB`), the present mode (e.g. `VK_PRESENT_MODE_FIFO_KHR` for vsync), and the image extent (window resolution).

```cpp
VkSwapchainCreateInfoKHR swapInfo{};
swapInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
swapInfo.surface          = surface;
swapInfo.minImageCount    = 2;                        // double-buffer
swapInfo.imageFormat      = VK_FORMAT_B8G8R8A8_SRGB;
swapInfo.imageColorSpace  = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
swapInfo.imageExtent      = {800, 600};
swapInfo.imageArrayLayers = 1;
swapInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
swapInfo.preTransform     = capabilities.currentTransform;
swapInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
swapInfo.presentMode      = VK_PRESENT_MODE_FIFO_KHR;
swapInfo.clipped          = VK_TRUE;

VkSwapchainKHR swapChain;
vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapChain);
```

### Step 6 — Image Views

A `VkImageView` tells Vulkan how to interpret each swap-chain image — its format, the aspect (colour), and how to map the channels.

```cpp
for (VkImage img : swapChainImages) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image    = img;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format   = swapChainImageFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView view;
    vkCreateImageView(device, &viewInfo, nullptr, &view);
    swapChainImageViews.push_back(view);
}
```

### Step 7 — Render Pass

A render pass describes the attachments (colour, depth, stencil) that the GPU renders into, along with what happens to them at the start (`VK_ATTACHMENT_LOAD_OP_CLEAR` = clear the image) and end (`VK_ATTACHMENT_STORE_OP_STORE` = keep the result for presentation) of the pass.

```cpp
VkAttachmentDescription colorAttachment{};
colorAttachment.format         = swapChainImageFormat;
colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;   // clear on begin
colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;  // keep
colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};

VkSubpassDescription subpass{};
subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
subpass.colorAttachmentCount = 1;
subpass.pColorAttachments    = &colorRef;

VkRenderPassCreateInfo rpInfo{};
rpInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
rpInfo.attachmentCount = 1;
rpInfo.pAttachments    = &colorAttachment;
rpInfo.subpassCount    = 1;
rpInfo.pSubpasses      = &subpass;

VkRenderPass renderPass;
vkCreateRenderPass(device, &rpInfo, nullptr, &renderPass);
```

### Step 8 — Shaders

The vertex shader passes positions straight through to clip space; the fragment shader outputs solid white. Shaders are written in GLSL and compiled offline to SPIR-V using `glslc` before your application loads them.

**vert.glsl**:
```glsl
#version 450
layout(location = 0) in vec4 position;
void main() {
    gl_Position = position;
}
```

**frag.glsl**:
```glsl
#version 450
layout(location = 0) out vec4 outColor;
void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0); // solid white
}
```

Compile them:
```sh
glslc vert.glsl -o vert.spv
glslc frag.glsl -o frag.spv
```

Load the SPIR-V bytecode at runtime and create shader modules:
```cpp
// Helper: read a binary file into a vector of bytes.
auto readFile = [](const std::string& path) {
    std::ifstream f(path, std::ios::ate | std::ios::binary);
    size_t size = f.tellg();
    std::vector<char> buf(size);
    f.seekg(0);
    f.read(buf.data(), size);
    return buf;
};

auto vertCode = readFile("vert.spv");
auto fragCode = readFile("frag.spv");

auto makeModule = [&](const std::vector<char>& code) {
    VkShaderModuleCreateInfo info{};
    info.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size();
    info.pCode    = reinterpret_cast<const uint32_t*>(code.data());
    VkShaderModule mod;
    vkCreateShaderModule(device, &info, nullptr, &mod);
    return mod;
};

VkShaderModule vertModule = makeModule(vertCode);
VkShaderModule fragModule = makeModule(fragCode);
```

### Step 9 — Graphics Pipeline

In Vulkan all fixed-function state is part of the pipeline object — topology, viewport, rasterizer, colour blend, etc. Creating it all at once enables the driver to fully optimise the pipeline upfront, with no surprises at draw time.

```cpp
// Shader stages
VkPipelineShaderStageCreateInfo stages[2] = {};
stages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
stages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
stages[0].module = vertModule;
stages[0].pName  = "main";
stages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
stages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
stages[1].module = fragModule;
stages[1].pName  = "main";

// Vertex input — one binding, one vec4 attribute at location 0
VkVertexInputBindingDescription binding{};
binding.binding   = 0;
binding.stride    = 4 * sizeof(float); // vec4
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

// Input assembly — triangle list topology
VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
inputAssembly.sType    = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

// Viewport and scissor
VkViewport viewport{0, 0, 800, 600, 0.0f, 1.0f};
VkRect2D   scissor{{0, 0}, {800, 600}};
VkPipelineViewportStateCreateInfo viewportState{};
viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
viewportState.viewportCount = 1;
viewportState.pViewports    = &viewport;
viewportState.scissorCount  = 1;
viewportState.pScissors     = &scissor;

// Rasterizer
VkPipelineRasterizationStateCreateInfo rasterizer{};
rasterizer.sType       = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
rasterizer.cullMode    = VK_CULL_MODE_BACK_BIT;
rasterizer.frontFace   = VK_FRONT_FACE_CLOCKWISE;
rasterizer.lineWidth   = 1.0f;

// Multisampling (disabled for simplicity)
VkPipelineMultisampleStateCreateInfo multisampling{};
multisampling.sType               = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

// Colour blend — no blending (opaque white triangle)
VkPipelineColorBlendAttachmentState blendAttachment{};
blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
                               | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

VkPipelineColorBlendStateCreateInfo colorBlend{};
colorBlend.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
colorBlend.attachmentCount   = 1;
colorBlend.pAttachments      = &blendAttachment;

// Pipeline layout (no descriptor sets for this chapter)
VkPipelineLayoutCreateInfo layoutInfo{};
layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
VkPipelineLayout pipelineLayout;
vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);

// Assemble the pipeline
VkGraphicsPipelineCreateInfo pipelineInfo{};
pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
pipelineInfo.stageCount          = 2;
pipelineInfo.pStages             = stages;
pipelineInfo.pVertexInputState   = &vertexInput;
pipelineInfo.pInputAssemblyState = &inputAssembly;
pipelineInfo.pViewportState      = &viewportState;
pipelineInfo.pRasterizationState = &rasterizer;
pipelineInfo.pMultisampleState   = &multisampling;
pipelineInfo.pColorBlendState    = &colorBlend;
pipelineInfo.layout              = pipelineLayout;
pipelineInfo.renderPass          = renderPass;
pipelineInfo.subpass             = 0;

VkPipeline graphicsPipeline;
vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr,
                           &graphicsPipeline);
```

### Step 10 — Framebuffers

A `VkFramebuffer` binds the image views from Step 6 to the render pass from Step 7. You need one framebuffer per swap-chain image.

```cpp
std::vector<VkFramebuffer> framebuffers(swapChainImageViews.size());
for (size_t i = 0; i < swapChainImageViews.size(); ++i) {
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass      = renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments    = &swapChainImageViews[i];
    fbInfo.width           = 800;
    fbInfo.height          = 600;
    fbInfo.layers          = 1;
    vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]);
}
```

### Step 11 — Vertex Data and Vertex Buffer

The triangle uses three four-component positions in clip space:

```cpp
const float vertexData[] = {
     0.0f,  0.5f,  0.0f,  1.0f,   // top
    -0.5f, -0.5f,  0.0f,  1.0f,   // bottom-left
     0.5f, -0.5f,  0.0f,  1.0f,   // bottom-right
};
```

To make this data available to the GPU, you create a `VkBuffer`, allocate `VkDeviceMemory`, bind them together, and then copy the data in via `vkMapMemory`:

```cpp
VkBufferCreateInfo bufInfo{};
bufInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufInfo.size  = sizeof(vertexData);
bufInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
bufInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

VkBuffer vertexBuffer;
vkCreateBuffer(device, &bufInfo, nullptr, &vertexBuffer);

// Query memory requirements and find a host-visible memory type.
VkMemoryRequirements memReq;
vkGetBufferMemoryRequirements(device, vertexBuffer, &memReq);

VkMemoryAllocateInfo allocInfo{};
allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
allocInfo.allocationSize  = memReq.size;
allocInfo.memoryTypeIndex = findMemoryType(physicalDevice, memReq.memoryTypeBits,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

VkDeviceMemory vertexBufferMemory;
vkAllocateMemory(device, &allocInfo, nullptr, &vertexBufferMemory);
vkBindBufferMemory(device, vertexBuffer, vertexBufferMemory, 0);

// Map the memory and copy the vertex data into it.
void* data;
vkMapMemory(device, vertexBufferMemory, 0, sizeof(vertexData), 0, &data);
memcpy(data, vertexData, sizeof(vertexData));
vkUnmapMemory(device, vertexBufferMemory);
```

### Step 12 — Command Pool and Command Buffers

Vulkan records commands into a `VkCommandBuffer` first, then submits the whole buffer to the GPU in one shot. This allows multi-threaded recording and efficient GPU scheduling.

```cpp
VkCommandPoolCreateInfo poolInfo{};
poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.queueFamilyIndex = graphicsFamily;
poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

VkCommandPool commandPool;
vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

std::vector<VkCommandBuffer> commandBuffers(framebuffers.size());
VkCommandBufferAllocateInfo cbAlloc{};
cbAlloc.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
cbAlloc.commandPool        = commandPool;
cbAlloc.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
cbAlloc.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());
vkAllocateCommandBuffers(device, &cbAlloc, commandBuffers.data());
```

### Step 13 — Recording Commands

For each swap-chain image, record: begin render pass → bind pipeline → bind vertex buffer → draw → end render pass.

```cpp
for (size_t i = 0; i < commandBuffers.size(); ++i) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(commandBuffers[i], &beginInfo);

    // Begin render pass — clears the framebuffer to the specified colour.
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass        = renderPass;
    rpBegin.framebuffer       = framebuffers[i];
    rpBegin.renderArea.extent = {800, 600};
    rpBegin.clearValueCount   = 1;
    rpBegin.pClearValues      = &clearColor;
    vkCmdBeginRenderPass(commandBuffers[i], &rpBegin,
                         VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffers[i],
                      VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, &vertexBuffer, &offset);

    // Draw 3 vertices as a triangle.
    vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffers[i]);
    vkEndCommandBuffer(commandBuffers[i]);
}
```

### Step 14 — Synchronisation and the Render Loop

Vulkan has no implicit synchronisation between CPU and GPU, or between the GPU and the display. You use semaphores to synchronise GPU operations with each other and fences to signal the CPU when the GPU has finished a frame.

```cpp
VkSemaphore imageAvailable, renderFinished;
VkFence     inFlightFence;

VkSemaphoreCreateInfo semInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                            nullptr,
                            VK_FENCE_CREATE_SIGNALED_BIT};
vkCreateSemaphore(device, &semInfo, nullptr, &imageAvailable);
vkCreateSemaphore(device, &semInfo, nullptr, &renderFinished);
vkCreateFence(device,     &fenceInfo, nullptr, &inFlightFence);

// Main render loop
while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();

    // Wait for the previous frame to finish (CPU-GPU sync via fence).
    vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &inFlightFence);

    // Acquire a swap-chain image to render into.
    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, swapChain, UINT64_MAX,
                          imageAvailable, VK_NULL_HANDLE, &imageIndex);

    // Submit the command buffer (GPU-GPU sync via semaphores).
    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount   = 1;
    submitInfo.pWaitSemaphores      = &imageAvailable;
    submitInfo.pWaitDstStageMask    = &waitStage;
    submitInfo.commandBufferCount   = 1;
    submitInfo.pCommandBuffers      = &commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &renderFinished;
    vkQueueSubmit(graphicsQueue, 1, &submitInfo, inFlightFence);

    // Present the rendered image to the screen.
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = &renderFinished;
    presentInfo.swapchainCount     = 1;
    presentInfo.pSwapchains        = &swapChain;
    presentInfo.pImageIndices      = &imageIndex;
    vkQueuePresentKHR(presentQueue, &presentInfo);
}
vkDeviceWaitIdle(device);
```

---

## Cleanup

Every Vulkan object must be destroyed in reverse creation order.

```cpp
vkDestroyFence(device, inFlightFence, nullptr);
vkDestroySemaphore(device, renderFinished, nullptr);
vkDestroySemaphore(device, imageAvailable, nullptr);
vkFreeMemory(device, vertexBufferMemory, nullptr);
vkDestroyBuffer(device, vertexBuffer, nullptr);
for (auto fb : framebuffers) vkDestroyFramebuffer(device, fb, nullptr);
vkDestroyPipeline(device, graphicsPipeline, nullptr);
vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
vkDestroyShaderModule(device, fragModule, nullptr);
vkDestroyShaderModule(device, vertModule, nullptr);
vkDestroyRenderPass(device, renderPass, nullptr);
for (auto iv : swapChainImageViews) vkDestroyImageView(device, iv, nullptr);
vkDestroySwapchainKHR(device, swapChain, nullptr);
vkDestroyCommandPool(device, commandPool, nullptr);
vkDestroySurfaceKHR(instance, surface, nullptr);
vkDestroyDevice(device, nullptr);
vkDestroyInstance(instance, nullptr);
glfwDestroyWindow(window);
glfwTerminate();
```

---

## The Complete Code

See [`code/Chapter02/main.cpp`](./code/Chapter02/main.cpp) for the full self-contained implementation that combines Chapters 1 and 2 into a program that opens a window and renders a white triangle.

---

[← Chapter 1: The Basics](./Chapter01_TheBasics.html) | [Chapter 3: Playing with Colors →](./Chapter03_Colors.html) *(coming soon)*
