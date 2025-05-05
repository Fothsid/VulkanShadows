///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Vertex shader used for drawing to the shadow map.
///
#version 450
#include "scene.glsl"

layout (location = 0) in  vec3 aPosition;
layout (location = 1) in  vec2 aTexCoord;

layout (location = 0) out vec2 outTexCoord;

void main() {
    outTexCoord = aTexCoord;
    gl_Position = camera.projView * transform * vec4(aPosition, 1);
}
