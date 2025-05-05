/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Storage for commonly used VkSamplers.
///
#pragma once

#include "Renderer.hpp"

enum eSampler {
    eSampler_Linear,
    eSampler_Nearest,
    eSampler_Shadow
};

struct CommonSamplers {
    CommonSamplers(Renderer& renderer);
    ~CommonSamplers();

    CommonSamplers(const CommonSamplers&) = delete;
    CommonSamplers(CommonSamplers&&);

    VkSampler createSampler(const VkSamplerCreateInfo& ci);
    void deleteSampler(VkSampler& sampler);

    VkSampler linear;
    VkSampler nearest;
    VkSampler shadow;
    VkSampler shadowLinear;

    Renderer& renderer;
};