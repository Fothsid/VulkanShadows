/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Storage for commonly used VkSamplers.
///
#include "Common.hpp"
#include "CommonSamplers.hpp"

static constexpr VkSamplerCreateInfo sampler_infos[] = {
    { // 0: linear
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,  // mipLodBias
        false, // anisotropyEnable
        0.0f,  // maxAnisotropy
        false, // compareEnable
        VK_COMPARE_OP_LESS,
        0.0f,  // minLod
        VK_LOD_CLAMP_NONE, // maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false, // unnormalizedCoordinates
    },
    { // 1: nearest
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0,
        VK_FILTER_NEAREST, VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        VK_SAMPLER_ADDRESS_MODE_REPEAT,
        0.0f,  // mipLodBias
        false, // anisotropyEnable
        0.0f,  // maxAnisotropy
        false, // compareEnable
        VK_COMPARE_OP_LESS,
        0.0f,  // minLod
        VK_LOD_CLAMP_NONE, // maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false, // unnormalizedCoordinates
    },
    { // 2: shadow binary
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0,
        VK_FILTER_NEAREST, VK_FILTER_NEAREST,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,  // mipLodBias
        false, // anisotropyEnable
        0.0f,  // maxAnisotropy
        true, // compareEnable
        VK_COMPARE_OP_GREATER,
        0.0f,  // minLod
        VK_LOD_CLAMP_NONE, // maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false, // unnormalizedCoordinates
    },
    { // 2: hardware shadow 2x2 percentage closer filtering
        VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, nullptr, 0,
        VK_FILTER_LINEAR, VK_FILTER_LINEAR,
        VK_SAMPLER_MIPMAP_MODE_NEAREST,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        0.0f,  // mipLodBias
        false, // anisotropyEnable
        0.0f,  // maxAnisotropy
        true, // compareEnable
        VK_COMPARE_OP_GREATER,
        0.0f,  // minLod
        VK_LOD_CLAMP_NONE, // maxLod
        VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
        false, // unnormalizedCoordinates
    },
};

CommonSamplers::CommonSamplers(Renderer& renderer)
    : renderer(renderer)
{
    linear       = createSampler(sampler_infos[0]);
    nearest      = createSampler(sampler_infos[1]);
    shadow       = createSampler(sampler_infos[2]);
    shadowLinear = createSampler(sampler_infos[3]);
}

CommonSamplers::~CommonSamplers() {
    deleteSampler(linear);
    deleteSampler(nearest);
    deleteSampler(shadow);
    deleteSampler(shadowLinear);
}

VkSampler CommonSamplers::createSampler(const VkSamplerCreateInfo& ci) {
    VkSampler sampler;
    VKCHECK(vkCreateSampler(renderer.getDevice(), &ci, nullptr, &sampler));
    return sampler;
}

void CommonSamplers::deleteSampler(VkSampler& sampler) {
    if (sampler) {
        vkDestroySampler(renderer.getDevice(), sampler, nullptr);
        sampler = VK_NULL_HANDLE;
    }
}

CommonSamplers::CommonSamplers(CommonSamplers&& orig)
    : renderer(orig.renderer)
    , linear(orig.linear)
    , nearest(orig.nearest)
    , shadow(orig.shadow)
    , shadowLinear(orig.shadowLinear)
{
    orig.linear = VK_NULL_HANDLE;
    orig.nearest = VK_NULL_HANDLE;
    orig.shadow = VK_NULL_HANDLE;
    orig.shadowLinear = VK_NULL_HANDLE;
}