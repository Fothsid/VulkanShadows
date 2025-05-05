/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkRenderPass object builder.
///
#include <cstring>
#include "Common.hpp"
#include "RenderPassBuilder.hpp"

VkRenderPass RenderPassBuilder::create(Renderer& renderer) {
    VkRenderPass renderPass;
    VKCHECK(vkCreateRenderPass(renderer.getDevice(), &ci, nullptr, &renderPass));
    return renderPass;
}

void RenderPassBuilder::reset() {
    ci = { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    ci.pAttachments  = attachments;
    ci.pSubpasses    = subpasses;
    ci.pDependencies = dependencies;

    memset(subpasses, 0, sizeof(subpasses));
    memset(attachments, 0, sizeof(attachments));
    memset(depthRefs, 0, sizeof(depthRefs));
    memset(colorRefs, 0, sizeof(colorRefs));
    memset(dependencies, 0, sizeof(dependencies));
}

uint32_t RenderPassBuilder::addAttachment(VkFormat format,
                                          VkAttachmentLoadOp loadOp,
                                          VkAttachmentStoreOp storeOp,
                                          VkImageLayout initialLayout,
                                          VkImageLayout finalLayout,
                                          int samples)
{
    if (ci.attachmentCount >= MAX_ATTACHMENTS) {
        throw std::runtime_error("Too many attachments.");
    }
    uint32_t index = ci.attachmentCount++;
    auto& att = attachments[index];
    switch (samples) {
    default:
    case  1: att.samples = VK_SAMPLE_COUNT_1_BIT;  break;
    case  2: att.samples = VK_SAMPLE_COUNT_2_BIT;  break;
    case  4: att.samples = VK_SAMPLE_COUNT_4_BIT;  break;
    case  8: att.samples = VK_SAMPLE_COUNT_8_BIT;  break;
    case 16: att.samples = VK_SAMPLE_COUNT_16_BIT; break;
    case 32: att.samples = VK_SAMPLE_COUNT_32_BIT; break;
    case 64: att.samples = VK_SAMPLE_COUNT_64_BIT; break;
    }
    att.format         = format;
    att.loadOp         = loadOp;
    att.storeOp        = storeOp;
    att.stencilLoadOp  = loadOp;
    att.stencilStoreOp = storeOp;
    att.initialLayout  = initialLayout;
    att.finalLayout    = finalLayout;
    return index;
}

uint32_t RenderPassBuilder::addSubpass() {
    if (ci.subpassCount >= MAX_SUBPASSES) {
        throw std::runtime_error("Too many subpasses.");
    }
    uint32_t index = ci.subpassCount++;
    auto& spd = subpasses[index];
    spd.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    spd.pColorAttachments = &colorRefs[index][0];
    spd.pDepthStencilAttachment = nullptr;
    return index;
}

void RenderPassBuilder::addSubpassColorAttachment(uint32_t subpass, uint32_t att, VkImageLayout layout) {
    if (subpass >= ci.subpassCount) {
        throw std::runtime_error("Tried adding a color attachment to a non-existing subpass.");
    }
    if (att >= ci.attachmentCount) {
        throw std::runtime_error("Tried creating an attachment reference to a non-existing attachment.");
    }
    auto& spd = subpasses[subpass];
    if (spd.colorAttachmentCount >= MAX_ATTACHMENT_REFERENCES) {
        throw std::runtime_error("Too many color attachment references!");
    }

    auto& ref = colorRefs[subpass][spd.colorAttachmentCount++];
    ref.attachment = att;
    ref.layout = layout;
}

void RenderPassBuilder::setSubpassDepthStencilAttachment(uint32_t subpass, uint32_t att, VkImageLayout layout) {
    if (subpass >= ci.subpassCount) {
        throw std::runtime_error("Tried setting a depth attachment to a non-existing subpass.");
    }
    if (att >= ci.attachmentCount) {
        throw std::runtime_error("Tried creating an attachment reference to a non-existing attachment.");
    }
    auto& ref = depthRefs[subpass];
    ref.attachment = att;
    ref.layout = layout;
    subpasses[subpass].pDepthStencilAttachment = &ref;
}

void RenderPassBuilder::addDependency(const VkSubpassDependency& d) {
    if (ci.dependencyCount >= MAX_DEPENDENCIES) {
        throw std::runtime_error("Too many dependencies.");
    }
    dependencies[ci.dependencyCount++] = d;
}