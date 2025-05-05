/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Vertex shader used for the shadow volumes pipelines.
///
#version 450
#include "scene.glsl"

layout (location = 0) in  vec3 aPosition;
layout (location = 0) out vec4 outPosition;
void main() {
    outPosition = transform * vec4(aPosition, 1);
    gl_Position = camera.projView * outPosition;
}
