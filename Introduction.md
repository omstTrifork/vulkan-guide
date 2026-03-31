# Introduction: The Environment

In the original [OpenGL Tutorial](https://paroj.github.io/gltut/index.html), the author uses **FreeGLUT**. In our modern Vulkan 1.4 journey on macOS, we use a more robust stack.

## The Rosetta Stone: Setup

| Feature | OpenGL (gltut) | Vulkan 1.4 (Our Guide) |
| :--- | :--- | :--- |
| **Windowing** | FreeGLUT | GLFW |
| **API Version** | OpenGL 3.3 | Vulkan 1.4.341 |
| **OS Layer** | Native Drivers | MoltenVK (Vulkan over Metal) |
| **Shader Language** | GLSL (Text) | SPIR-V (Binary) |

## Hardware Verification
To ensure our journey begins on solid ground, we verified the environment via the terminal:

- **GPU:** Apple M1 Pro
- **SDK Path:** `~/VulkanSDK/vulkan/macOS`
- **Instance Version:** 1.4.341

---
[Next Chapter: Hello Triangle](./Chapter01_Instance.html)
