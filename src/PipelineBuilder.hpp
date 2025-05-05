/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkPipeline object builder.
///
#pragma once
#include "Renderer.hpp"
#include "Vertex.hpp"

class PipelineBuilder {
public:
    static const int MAX_SHADER_STAGES     = 3;
    static const int MAX_DYNAMIC_STATES    = 4;
    static const int MAX_VERTEX_BINDINGS   = 4;
    static const int MAX_VERTEX_ATTRIBUTES = MAX_VERTEX_BINDINGS*8;
    static const int MAX_BLEND_ATTACHMENTS = 4;

    PipelineBuilder() {
        reset();
    }

    VkPipeline create(Renderer& renderer, VkPipelineCache cache = VK_NULL_HANDLE);
    void reset();

    void clearShaderStages();
    void addShaderStage(VkShaderStageFlags stage, VkShaderModule module, const VkSpecializationInfo* spec = nullptr);
    void addVertexShader(VkShaderModule module, const VkSpecializationInfo* spec = nullptr) {
        addShaderStage(VK_SHADER_STAGE_VERTEX_BIT, module, spec);
    }
    void addGeometryShader(VkShaderModule module, const VkSpecializationInfo* spec = nullptr) {
        addShaderStage(VK_SHADER_STAGE_GEOMETRY_BIT, module, spec);
    }
    void addFragmentShader(VkShaderModule module, const VkSpecializationInfo* spec = nullptr) {
        addShaderStage(VK_SHADER_STAGE_FRAGMENT_BIT, module, spec);
    }

    void setRenderPass(VkRenderPass renderPass, int subpassIndex = 0);
    void setLayout(VkPipelineLayout layout);

    void clearDynamicStates();
    void addDynamicState(VkDynamicState state);

    void setViewport(float x, float y, float w, float h, float minDepth, float maxDepth);
    void setScissor(int x, int y, int w, int h);

    void setPrimitive(VkPrimitiveTopology prim, bool primRestart = false);
    void setPolyMode(VkPolygonMode mode);
    void setCulling(VkCullModeFlags culling, VkFrontFace frontFace);
    void setDepthClamp(bool enable);
    void setLineWidth(float width = 1.0f);
    void setMultisampling(int count = 1);
    void setDepthState(bool depthTest, bool depthWrite, VkCompareOp op = VK_COMPARE_OP_LESS);
    void setStencilState(bool stencilTest, const VkStencilOpState& front = {}, const VkStencilOpState& back = {});
    void setDepthBias(bool enable, float constant = 0.001, float slope = 0, float clamp = 0);

    void clearVertexBindings();
    void addVertexBinding(uint32_t binding, uint32_t stride, bool instance = false);
    void addVertexAttribute(uint32_t location, uint32_t binding, VkFormat format, uint32_t offset);
    void addVertexAttributesFromFlags(uint32_t binding, eVertexFlags flags, eVertexFlags ignore = eVertexFlags_None);

    // NOTE:
    // Color formula is as follows:
    // finalColor = (srcFactor * newColor) <op> (dstFactor * oldColor);
    void clearBlendAttachments();
    void addBlendAttachment(bool enable,
                            VkBlendOp     colorOp,
                            VkBlendFactor colorSrcFactor,
                            VkBlendFactor colorDstFactor,
                            VkBlendOp     alphaOp,
                            VkBlendFactor alphaSrcFactor,
                            VkBlendFactor alphaDstFactor);
    void addBlendAttachment(bool enable,
                            VkBlendOp     op        = VK_BLEND_OP_ADD,
                            VkBlendFactor srcFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                            VkBlendFactor dstFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA)
    {
        addBlendAttachment(enable,
                           op, srcFactor, dstFactor,
                           op, srcFactor, dstFactor);
    }

private:
    VkGraphicsPipelineCreateInfo           ci;

    VkPipelineDynamicStateCreateInfo       dynamicState;
    VkDynamicState                         dynamicStates[MAX_DYNAMIC_STATES];

    VkPipelineShaderStageCreateInfo        shaderStages[MAX_SHADER_STAGES];
    
    VkPipelineVertexInputStateCreateInfo   vertexInputState;
    VkVertexInputBindingDescription        vertexBindings[MAX_VERTEX_BINDINGS];
    VkVertexInputAttributeDescription      vertexAttributes[MAX_VERTEX_ATTRIBUTES];
   
    VkPipelineInputAssemblyStateCreateInfo inputAssembly;
    VkPipelineRasterizationStateCreateInfo rasterizationState;
    VkPipelineDepthStencilStateCreateInfo  depthStencilState;
    VkPipelineMultisampleStateCreateInfo   multisampleState;
    
    VkPipelineColorBlendStateCreateInfo    blendState;
    VkPipelineColorBlendAttachmentState    blendAttachments[MAX_BLEND_ATTACHMENTS];

    VkPipelineViewportStateCreateInfo      viewportState;
    VkViewport                             viewport;
    VkRect2D                               scissor;
};
