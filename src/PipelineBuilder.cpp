/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkPipeline object builder.
///
#include "Common.hpp"
#include <cstring>
#include "PipelineBuilder.hpp"

VkPipeline PipelineBuilder::create(Renderer& renderer, VkPipelineCache cache) {
    VkPipeline pipeline;
    VKCHECK(vkCreateGraphicsPipelines(renderer.getDevice(), cache, 1, &ci, nullptr, &pipeline));
    return pipeline;
}

void PipelineBuilder::reset() {
    ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pStages             = shaderStages;
    ci.pVertexInputState   = &vertexInputState;
    ci.pInputAssemblyState = &inputAssembly;
    ci.pViewportState      = &viewportState;
    ci.pRasterizationState = &rasterizationState;
    ci.pMultisampleState   = &multisampleState;
    ci.pDepthStencilState  = &depthStencilState;
    ci.pColorBlendState    = &blendState;
    ci.pDynamicState       = &dynamicState;

    memset(shaderStages, 0, sizeof(shaderStages));

    vertexInputState = { VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    vertexInputState.pVertexBindingDescriptions   = vertexBindings;
    vertexInputState.pVertexAttributeDescriptions = vertexAttributes;
    memset(vertexBindings, 0, sizeof(vertexBindings));
    memset(vertexAttributes, 0, sizeof(vertexAttributes));
   
    inputAssembly      = { VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
    rasterizationState = { VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO  };
    depthStencilState  = { VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO  };

    blendState = { VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO };
    blendState.pAttachments = blendAttachments;
    memset(blendAttachments, 0, sizeof(blendAttachments));

    viewportState = { VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
    viewportState.pViewports    = &viewport;
    viewportState.pScissors     = &scissor;
    viewportState.viewportCount = 1;
    viewportState.scissorCount  = 1;

    dynamicState = { VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
    dynamicState.pDynamicStates = dynamicStates;
    memset(dynamicStates, 0, sizeof(dynamicStates));

    multisampleState = { VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };

    // Default settings
    setPrimitive(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    setDepthState(false, false);
    setStencilState(false, {}, {});
    setViewport(0, 0, 1, 1, 0, 1);
    setScissor(0, 0, 1, 1);
    setLineWidth(1.0f);
    setMultisampling(1);
    setPolyMode(VK_POLYGON_MODE_FILL);
}

void PipelineBuilder::clearShaderStages() {
    ci.stageCount = 0;
}

void PipelineBuilder::addShaderStage(VkShaderStageFlags stage, VkShaderModule module, const VkSpecializationInfo* spec) {
    if (ci.stageCount >= MAX_SHADER_STAGES) {
        throw std::runtime_error("Too many shader stages.");
    }

    int i = ci.stageCount++;
    shaderStages[i].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[i].stage               = (VkShaderStageFlagBits) stage; // MSVC rants about narrowing conversions.
    shaderStages[i].module              = module;
    shaderStages[i].pName               = "main";
    shaderStages[i].pSpecializationInfo = spec;
}

void PipelineBuilder::setRenderPass(VkRenderPass renderPass, int subpassIndex) {
    ci.renderPass = renderPass;
    ci.subpass = subpassIndex;
}

void PipelineBuilder::setLayout(VkPipelineLayout layout) {
    ci.layout = layout;
}

void PipelineBuilder::clearDynamicStates() {
    dynamicState.dynamicStateCount = 0;
}

void PipelineBuilder::addDynamicState(VkDynamicState state) {
    if (dynamicState.dynamicStateCount >= MAX_DYNAMIC_STATES) {
        throw std::runtime_error("Too many dynamic states.");
    }
    dynamicStates[dynamicState.dynamicStateCount++] = state; 
}

void PipelineBuilder::setViewport(float x, float y, float w, float h, float minDepth, float maxDepth) {
    viewport.x        = x;
    viewport.y        = y;
    viewport.width    = w;
    viewport.height   = h;
    viewport.minDepth = minDepth;
    viewport.maxDepth = maxDepth;
}

void PipelineBuilder::setScissor(int x, int y, int w, int h) {
    scissor.offset.x      = x;
    scissor.offset.y      = y;
    scissor.extent.width  = w;
    scissor.extent.height = h;
}

void PipelineBuilder::setPrimitive(VkPrimitiveTopology prim, bool primRestart) {
    inputAssembly.topology               = prim;
    inputAssembly.primitiveRestartEnable = primRestart;
}

void PipelineBuilder::setPolyMode(VkPolygonMode mode) {
    rasterizationState.polygonMode = mode;
}

void PipelineBuilder::setCulling(VkCullModeFlags culling, VkFrontFace frontFace) {
    rasterizationState.cullMode  = culling;
    rasterizationState.frontFace = frontFace;
}

void PipelineBuilder::setDepthClamp(bool enable) {
    rasterizationState.depthClampEnable = enable;
}

void PipelineBuilder::setDepthBias(bool enable, float constant, float slope, float clamp) {
    rasterizationState.depthBiasEnable         = enable;
    rasterizationState.depthBiasConstantFactor = constant;
    rasterizationState.depthBiasClamp          = clamp;
    rasterizationState.depthBiasSlopeFactor    = slope;
}

void PipelineBuilder::setLineWidth(float width) {
    rasterizationState.lineWidth = width;
}

void PipelineBuilder::setMultisampling(int count) {
    switch (count) {
    default:
    case  1: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  break;
    case  2: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_2_BIT;  break;
    case  4: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;  break;
    case  8: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_8_BIT;  break;
    case 16: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_16_BIT; break;
    case 32: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_32_BIT; break;
    case 64: multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_64_BIT; break;
    }
}

void PipelineBuilder::setDepthState(bool depthTest, bool depthWrite, VkCompareOp op) {
    depthStencilState.depthCompareOp   = op;
    depthStencilState.depthTestEnable  = depthTest;
    depthStencilState.depthWriteEnable = depthWrite;
}

void PipelineBuilder::setStencilState(bool stencilTest, const VkStencilOpState& front, const VkStencilOpState& back) {
    depthStencilState.stencilTestEnable = stencilTest;
    depthStencilState.front             = front;
    depthStencilState.back              = back;
}

void PipelineBuilder::clearVertexBindings() {
    vertexInputState.vertexBindingDescriptionCount   = 0;
    vertexInputState.vertexAttributeDescriptionCount = 0;
}

void PipelineBuilder::addVertexBinding(uint32_t binding, uint32_t stride, bool instance) {
    if (vertexInputState.vertexBindingDescriptionCount >= MAX_VERTEX_BINDINGS) {
        throw std::runtime_error("Too many vertex buffer bindings.");
    }

    auto& b = vertexBindings[vertexInputState.vertexBindingDescriptionCount++];
    b.binding   = binding;
    b.stride    = stride;
    b.inputRate = instance
                ? VK_VERTEX_INPUT_RATE_INSTANCE
                : VK_VERTEX_INPUT_RATE_VERTEX;
}

void PipelineBuilder::addVertexAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset) {
    if (vertexInputState.vertexAttributeDescriptionCount >= MAX_VERTEX_ATTRIBUTES) {
        throw std::runtime_error("Too many vertex attributes.");
    }

    auto& att = vertexAttributes[vertexInputState.vertexAttributeDescriptionCount++];
    att.location = location;
    att.binding  = binding;
    att.format   = format;
    att.offset   = offset;
}

void PipelineBuilder::addVertexAttributesFromFlags(uint32_t binding, eVertexFlags flags, eVertexFlags ignore) {
    uint32_t offset = 0;
    uint32_t location = 0;

    addVertexAttribute(location++, binding, VK_FORMAT_R32G32B32_SFLOAT, offset);
    offset += sizeof(VertexNTC::position);

    if (flags & eVertexFlags_Normal) {
        if ((ignore & eVertexFlags_Normal) == 0) {
            addVertexAttribute(location++, binding, VK_FORMAT_R32G32B32_SFLOAT, offset);
        }
        offset += sizeof(VertexNTC::normal);
    }

    if (flags & eVertexFlags_TexCoord) {
        if ((ignore & eVertexFlags_TexCoord) == 0) {
            addVertexAttribute(location++, binding, VK_FORMAT_R32G32_SFLOAT, offset);
        }
        offset += sizeof(VertexNTC::texCoord);
    }

    if (flags & eVertexFlags_Color) {
        if ((ignore & eVertexFlags_Color) == 0) {
            addVertexAttribute(location++, binding, VK_FORMAT_R32G32B32A32_SFLOAT, offset);
        }
        offset += sizeof(VertexNTC::color);
    }
}

void PipelineBuilder::clearBlendAttachments() {
    blendState.attachmentCount = 0;
}

void PipelineBuilder::addBlendAttachment(bool enable,
                                         VkBlendOp     colorOp,
                                         VkBlendFactor colorSrcFactor,
                                         VkBlendFactor colorDstFactor,
                                         VkBlendOp     alphaOp,
                                         VkBlendFactor alphaSrcFactor,
                                         VkBlendFactor alphaDstFactor)
{
    if (blendState.attachmentCount >= MAX_BLEND_ATTACHMENTS) {
        throw std::runtime_error("Too many blend attachments.");
    }

    auto& att = blendAttachments[blendState.attachmentCount++];
    att.blendEnable         = enable;
    att.colorBlendOp        = colorOp;
    att.srcColorBlendFactor = colorSrcFactor;
    att.dstColorBlendFactor = colorDstFactor;
    att.alphaBlendOp        = alphaOp;
    att.srcAlphaBlendFactor = alphaSrcFactor;
    att.dstAlphaBlendFactor = alphaDstFactor;
    att.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT
                            | VK_COLOR_COMPONENT_G_BIT
                            | VK_COLOR_COMPONENT_B_BIT
                            | VK_COLOR_COMPONENT_A_BIT;
}
