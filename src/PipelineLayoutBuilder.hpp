/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkPipelineLayout object builder.
///
#pragma once
#include "Renderer.hpp"

class PipelineLayoutBuilder {
public:
    static const int MAX_SET_LAYOUTS = 8;
    static const int MAX_PUSH_CONSTANT_RANGES = 1;

    PipelineLayoutBuilder() {
        reset();
    }

    VkPipelineLayout create(Renderer& renderer);
    void reset();

    void addDescriptorSetLayout(VkDescriptorSetLayout layout);
    void addPushConstantRange(VkShaderStageFlags stage, uint32_t offset, uint32_t size);
private:
    VkPipelineLayoutCreateInfo ci;
    VkDescriptorSetLayout      layouts[MAX_SET_LAYOUTS];
    VkPushConstantRange        pcRanges[MAX_PUSH_CONSTANT_RANGES];
};
