# Chapter 1: The Basics — Creating a Vulkan Instance

The original gltut **[Tutorial 01 – Hello, Triangle!](https://github.com/paroj/gltut/blob/master/Documents/Basics/Tutorial%2001.xml)** draws a single white triangle to the screen using FreeGLUT for windowing, a hand-written GLSL vertex and fragment shader, a Vertex Buffer Object (VBO) for the position data, and `glDrawArrays` to issue the draw call. It is the minimum viable OpenGL program.

Vulkan requires the same logical steps, but every step that OpenGL performs implicitly must be done *explicitly*. This chapter focuses on the very first of those steps — creating a `VkInstance` — which is the Vulkan equivalent of initializing FreeGLUT and obtaining an OpenGL context.

## The Rosetta Stone: Chapter 1

The table below maps each concept from gltut Tutorial 01 to its Vulkan 1.4 counterpart.

| Concept | OpenGL / FreeGLUT (gltut Tut 01) | Vulkan 1.4 (Our Guide) |
| :--- | :--- | :--- |
| **Windowing library** | FreeGLUT (`glutInit`, `glutCreateWindow`) | GLFW (`glfwInit`, `glfwCreateWindow`) |
| **API context / root object** | Implicit OpenGL context created by FreeGLUT | Explicit `VkInstance` via `vkCreateInstance()` |
| **macOS compatibility** | Native OpenGL (deprecated since macOS 10.14) | MoltenVK — Vulkan over Metal; requires portability extensions |
| **Platform extensions** | None required | `VK_KHR_portability_enumeration` + `VK_EXT_metal_surface` + `VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR` |
| **Callback / loop model** | `glutDisplayFunc`, `glutMainLoop` | `while (!glfwWindowShouldClose(w)) { glfwPollEvents(); }` |
| **Screen clear** | `glClearColor(0,0,0,0); glClear(GL_COLOR_BUFFER_BIT)` | `VkRenderPass` load-op `VK_ATTACHMENT_LOAD_OP_CLEAR` with `VkClearValue` |
| **Vertex data** | `const float vertexPositions[]` (CPU array) | Same CPU array — uploaded into a `VkBuffer` backed by `VkDeviceMemory` |
| **GPU buffer allocation** | `glGenBuffers` / `glBindBuffer` / `glBufferData` | `vkCreateBuffer` + `vkAllocateMemory` + `vkBindBufferMemory` + `vkMapMemory` |
| **Vertex layout description** | `glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0)` | `VkVertexInputAttributeDescription` + `VkPipelineVertexInputStateCreateInfo` |
| **Vertex shader language** | GLSL text, runtime-compiled by the driver | SPIR-V binary, pre-compiled offline (e.g. `glslc vert.glsl -o vert.spv`) |
| **gltut vertex shader** | `layout(location=0) in vec4 position; gl_Position = position;` | `layout(location=0) in vec4 position; gl_Position = position;` (identical GLSL, different compile step) |
| **Fragment shader** | `out vec4 outputColor; outputColor = vec4(1,1,1,1);` | Same GLSL logic, compiled to SPIR-V |
| **Shader compilation** | `glCreateShader` / `glShaderSource` / `glCompileShader` | `vkCreateShaderModule(device, &{SPIR-V bytes}, ...)` |
| **Shader linking** | `glCreateProgram` / `glAttachShader` / `glLinkProgram` | `VkGraphicsPipelineCreateInfo` (vertex + fragment stages wired together in one `vkCreateGraphicsPipelines` call) |
| **Draw call** | `glDrawArrays(GL_TRIANGLES, 0, 3)` | `vkCmdDraw(commandBuffer, 3, 1, 0, 0)` |
| **Present / swap** | `glutSwapBuffers()` | `vkQueuePresentKHR(presentQueue, &presentInfo)` |
| **Viewport** | `glViewport(0, 0, w, h)` called in `reshape` | `VkViewport` set as part of the pipeline or dynamic state |
| **Error handling** | `glGetError()` (polled) | `VkResult` returned from every call — checked immediately |
| **Debug / validation** | Driver-side only (no portable layer) | `VK_LAYER_KHRONOS_validation` in debug builds |
| **Cleanup** | `glutDestroyWindow()` + implicit GL context teardown | `vkDestroyInstance()` + `glfwDestroyWindow()` + `glfwTerminate()` |

## Why So Many More Steps?

In gltut Tutorial 01, FreeGLUT hides a large amount of work: it opens the display connection, creates a window, negotiates a framebuffer format, and creates an OpenGL context — all in a few lines. OpenGL then implicitly inherits that context.

Vulkan exposes every one of those steps:

1. **Instance** — connect to the Vulkan runtime and enumerate GPUs.
2. **Surface** — connect the Vulkan Instance to the OS window (covered in Chapter 2).
3. **Physical device selection** — choose a GPU.
4. **Logical device + queues** — open a connection to the GPU.
5. **Swap chain** — the equivalent of FreeGLUT's double-buffered window.
6. **Render pass + framebuffers** — describe what the GPU renders into.
7. **Pipeline** — compile shaders and describe all fixed-function state.
8. **Command buffers** — record GPU commands explicitly.
9. **Synchronisation** — semaphores and fences to coordinate CPU/GPU/present.

**This chapter covers step 1 only.** Each subsequent chapter adds one or two steps until the triangle appears on screen.

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
