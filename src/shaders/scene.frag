///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Fragment shader for lit scene.
///
#version 450

#include "constants.glsl"
#include "scene.glsl"
#include "lighting.glsl"

layout (location = 0) in vec4 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec2 inTexCoord;

layout (location = 0) out vec4 outColor;

void main() {
    LightResult lighting = calculateLighting(inPosition, inNormal);

    vec3 final = vec3(0,0,0);
    if (OUTPUT_AMBIENT > 0) {
        final += lighting.ambient;    
    }
    if (OUTPUT_DIFFUSE > 0) {
        final += lighting.diffuse;    
    }
    outColor = sampleBaseColor(inTexCoord) * vec4(final, 1.0);
}
