# Chapter 1: The Basics — Creating a Vulkan Instance

This chapter covers the very first step in any Vulkan program: creating a `VkInstance`. The instance is Vulkan's root object — it connects your application to the Vulkan runtime and allows you to enumerate the GPUs available on the system.

## What Vulkan Requires Up Front

Unlike higher-level graphics APIs, Vulkan requires you to be explicit about everything from the start. Initialising a Vulkan program involves nine distinct steps; this chapter handles just the first one. Subsequent chapters add the remaining steps one or two at a time until a triangle appears on screen.

The nine steps are:

1. **Instance** — connect to the Vulkan runtime and enumerate GPUs. *(this chapter)*
2. **Surface** — connect the Vulkan instance to the OS window.
3. **Physical device selection** — choose a GPU.
4. **Logical device + queues** — open a connection to the GPU.
5. **Swap chain** — the double-buffered display mechanism.
6. **Render pass + framebuffers** — describe what the GPU renders into.
7. **Pipeline** — compile shaders and describe all fixed-function state.
8. **Command buffers** — record GPU commands explicitly.
9. **Synchronisation** — semaphores and fences to coordinate CPU, GPU, and the display.

## Key macOS / MoltenVK Details

MoltenVK translates Vulkan calls into Apple Metal. Because it does not implement every Vulkan feature, it is a *portability-subset* device. Two things are required to use it:

```cpp
// 1. Request the portability enumeration extension so MoltenVK is visible.
extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
extensions.push_back("VK_EXT_metal_surface");

// 2. Set the flag that opts in to enumerating portability-subset devices.
createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
```

Without these two items, `vkCreateInstance` returns `VK_ERROR_INCOMPATIBLE_DRIVER` on macOS.

## The Code
See [`code/Chapter01/main.cpp`](./code/Chapter01/main.cpp) for the implementation of step 1: creating a Vulkan 1.4 Instance with GLFW and all macOS portability extensions enabled.

---
[← Introduction](./Introduction.html) | [Chapter 2: Hello, Triangle! →](./Chapter02_Triangle.html)
