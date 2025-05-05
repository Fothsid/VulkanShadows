/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Helper definitions and functions for working with bindless textures.
///
#ifndef TEXTURES_GLSL
#define TEXTURES_GLSL

#extension GL_EXT_nonuniform_qualifier : enable

#define LinearSampler  (0)
#define NearestSampler (1)
#define ShadowSampler  (2)

layout (set = 0, binding = 0) uniform sampler     samplers[];
layout (set = 0, binding = 1) uniform texture2D   textures[];
layout (set = 0, binding = 1) uniform textureCube textureCubes[];

vec4 sampleTexture2D(uint index, uint samplerIndex, vec2 uv) {
    return texture(nonuniformEXT(sampler2D(textures[index], samplers[samplerIndex])), uv);
}

float sampleTextureCubeShadow(uint index, uint samplerIndex, vec4 p) {
    return texture(nonuniformEXT(samplerCubeShadow(textureCubes[index], samplers[samplerIndex])), p).x;
}

#endif