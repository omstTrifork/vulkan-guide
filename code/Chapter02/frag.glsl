/*
 * frag.glsl — fragment shader for Chapter 02: Hello, Triangle!
 *
 * Identical to the gltut Tutorial 01 fragment shader.
 * Outputs solid white for every fragment.
 *
 * Compile with:  glslc frag.glsl -o frag.spv
 */
#version 450

layout(location = 0) out vec4 outColor;

void main() {
    outColor = vec4(1.0, 1.0, 1.0, 1.0); // solid white
}
