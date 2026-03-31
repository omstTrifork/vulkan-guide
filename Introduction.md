# Introduction: The Environment

Before we go into the basics of writing a Vulkan program, we need to talk about the programming environment we will be working in.

The original [OpenGL tutorial series (gltut)](https://paroj.github.io/gltut/index.html) uses **FreeGLUT** for windowing and **OpenGL 3.3** as the graphics API, with shaders written in GLSL and compiled at runtime by the driver. Our Vulkan guide uses a structurally equivalent but more explicit stack.

## The Tools We Use

| Feature | OpenGL (gltut) | Vulkan 1.4 (Our Guide) |
| :--- | :--- | :--- |
| **Windowing** | FreeGLUT (`glutInit`, `glutCreateWindow`) | GLFW (`glfwInit`, `glfwCreateWindow`) |
| **Graphics API** | OpenGL 3.3 | Vulkan 1.4 |
| **Shader language** | GLSL — text, runtime-compiled by the driver | GLSL — compiled offline to SPIR-V binary with `glslc` |
| **macOS support** | Native OpenGL (deprecated since macOS 10.14) | MoltenVK — Vulkan translated to Metal |

**GLFW** is a lightweight, cross-platform library that handles window creation and input events. It has explicit support for Vulkan — unlike FreeGLUT, it will not create an OpenGL context unless you ask for one. Call `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` before creating your window.

**The Vulkan SDK** (from LunarG) provides the Vulkan headers, validation layers, and the `glslc` shader compiler. Download it from [vulkan.lunarg.com](https://vulkan.lunarg.com/) and follow the platform-specific instructions to set `VULKAN_SDK` in your environment.

**MoltenVK** (macOS only) is a Vulkan implementation that translates Vulkan calls into Apple Metal at runtime. It is bundled with the macOS Vulkan SDK. Because MoltenVK does not implement every optional Vulkan feature, it is classed as a *portability-subset* device — Chapter 1 explains the two extra lines of code this requires.

## Getting the Code

All source files for this guide live in the `code/` directory, one sub-folder per chapter:

```
code/
  Chapter01/main.cpp   ← VkInstance creation
  Chapter02/main.cpp   ← Complete Hello Triangle
```

## Building an Example

Each chapter folder contains a single self-contained `main.cpp`. Build it with your C++ compiler. For example on macOS with the Vulkan SDK and GLFW installed via Homebrew:

```sh
clang++ -std=c++17 code/Chapter01/main.cpp \
    -I$VULKAN_SDK/include \
    -L$VULKAN_SDK/lib -lvulkan \
    $(pkg-config --cflags --libs glfw3) \
    -o chapter01
./chapter01
```

On Linux, replace the MoltenVK-specific flags with your system's Vulkan library path.

---
[Next: Chapter 1 — The Basics](./Chapter01_TheBasics.html)
