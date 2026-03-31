# Chapter 1: The Basics — Creating a Vulkan Instance

In the original [OpenGL Tutorial – Chapter 1](https://paroj.github.io/gltut/Basics/Introduction.html), the author creates a **FreeGLUT** window and obtains an OpenGL context in just a few calls. Vulkan is more explicit: we must manually create an *Instance*, enumerate extensions, and satisfy the platform portability requirements before the GPU will talk to us.

## The Rosetta Stone: Chapter 1

| Concept | OpenGL (gltut) | Vulkan 1.4 (Our Guide) |
| :--- | :--- | :--- |
| **Library init** | `glutInit()` | Vulkan loader (dynamic library, no explicit init call) |
| **Window creation** | `glutCreateWindow()` | `glfwCreateWindow()` |
| **Context / Instance** | Implicit OpenGL context | Explicit `VkInstance` via `vkCreateInstance()` |
| **Extension model** | Driver-reported via `glGetString(GL_EXTENSIONS)` | Instance extensions: `VK_KHR_surface`, `VK_EXT_metal_surface`, `VK_KHR_portability_enumeration` |
| **macOS layer** | Native OpenGL (deprecated) | MoltenVK — Vulkan over Metal, requires portability flag |
| **Error handling** | `glGetError()` | `VkResult` checked on every call |
| **Validation** | Driver-side only | `VK_LAYER_KHRONOS_validation` layer (debug builds) |
| **Main loop** | `glutMainLoop()` | `while (!glfwWindowShouldClose(window)) { glfwPollEvents(); }` |
| **Cleanup** | `glutDestroyWindow()` | `vkDestroyInstance()` + `glfwDestroyWindow()` |

## Key Vulkan 1.4 Concepts Introduced

### 1. The Instance
A `VkInstance` is the root Vulkan object. It connects your application to the Vulkan runtime and lets you query available GPUs (*physical devices*). Every Vulkan program starts here.

### 2. Instance Extensions
Vulkan has zero built-in windowing support — surface extensions bridge the gap:

| Extension | Purpose |
| :--- | :--- |
| `VK_KHR_surface` | Base surface abstraction |
| `VK_EXT_metal_surface` | macOS/iOS Metal back-end |
| `VK_KHR_portability_enumeration` | Required by MoltenVK to enumerate the device |

GLFW reports the extensions it needs via `glfwGetRequiredInstanceExtensions()`, so we merge those with our macOS-specific additions.

### 3. Portability Enumeration Flag
MoltenVK exposes itself as a *portability subset* device — a Vulkan implementation that deliberately omits some features. Vulkan 1.4 requires you to explicitly opt-in to enumerating such devices:

```cpp
createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
```

Without this flag, `vkCreateInstance` returns `VK_ERROR_INCOMPATIBLE_DRIVER` on macOS.

### 4. Validation Layers (Debug Only)
The `VK_LAYER_KHRONOS_validation` layer intercepts every Vulkan call and emits warnings for incorrect usage. It is invaluable during development and is stripped from release builds.

## The Code
See [`code/Chapter01/main.cpp`](./code/Chapter01/main.cpp) for the full implementation.

---
[← Introduction](./Introduction.html) | [Chapter 2: The Triangle →](./Chapter02_Triangle.html) *(coming soon)*
