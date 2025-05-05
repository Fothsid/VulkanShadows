///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Fragment shader used for drawing to the shadow map.
///
#version 450

#include "constants.glsl"
#include "scene.glsl"
#include "lighting.glsl"

layout (location = 0) in vec2 inTexCoord;
void main() {
    if (ENABLE_ALPHA_TEST != 0) {
        sampleBaseColor(inTexCoord);
    }
}
