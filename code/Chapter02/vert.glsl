/*
 * vert.glsl — vertex shader for Chapter 02: Hello, Triangle!
 *
 * Identical to the gltut Tutorial 01 vertex shader.
 * Passes the clip-space position straight through to the rasteriser.
 *
 * Compile with:  glslc vert.glsl -o vert.spv
 */
#version 450

layout(location = 0) in vec4 position;

void main() {
    gl_Position = position;
}
