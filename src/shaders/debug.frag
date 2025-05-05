///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Fragment shader used for silhouette debug lines.
///
#version 450
layout (location = 0) out vec4 outColor;
layout (location = 0) in vec4 inColor;
void main() {
    outColor = inColor;
}
