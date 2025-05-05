/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkPipelineLayout object builder.
///
#include <cstring>
#include "Common.hpp"
#include "PipelineLayoutBuilder.hpp"

VkPipelineLayout PipelineLayoutBuilder::create(Renderer& renderer) {
    VkPipelineLayout pipelineLayout;
    VKCHECK(vkCreatePipelineLayout(renderer.getDevice(), &ci, nullptr, &pipelineLayout));
    return pipelineLayout;
}

void PipelineLayoutBuilder::reset() {
    ci = { VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO };
    ci.pSetLayouts         = layouts;
    ci.pPushConstantRanges = pcRanges;

    memset(layouts, 0, sizeof(layouts));
    memset(pcRanges, 0, sizeof(pcRanges));
}

void PipelineLayoutBuilder::addDescriptorSetLayout(VkDescriptorSetLayout layout) {
    if (ci.setLayoutCount >= MAX_SET_LAYOUTS) {
        throw std::runtime_error("Too many descriptor set layouts.");
    }
    layouts[ci.setLayoutCount++] = layout;
}

void PipelineLayoutBuilder::addPushConstantRange(VkShaderStageFlags stage, uint32_t offset, uint32_t size) {
    if (ci.pushConstantRangeCount >= MAX_PUSH_CONSTANT_RANGES) {
        throw std::runtime_error("Too many push constant ranges.");
    }
    pcRanges[ci.pushConstantRangeCount++] = {
        stage, offset, size
    };
}
