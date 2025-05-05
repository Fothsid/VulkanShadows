///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Vertex shader used for the lit scene.
///
#version 450

#include "scene.glsl"

layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;

layout (location = 0) out vec4 outPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec2 outTexCoord;

void main() {
    outNormal   = normalize(mat3(transpose(inverse(transform))) * aNormal); // For non-uniformly scaled transforms.
    outTexCoord = aTexCoord;
    outPosition = transform * vec4(aPosition, 1);
    gl_Position = camera.projView * outPosition;
}
