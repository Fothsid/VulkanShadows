/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkRenderPass object builder.
///
#pragma once
#include "Renderer.hpp"

class RenderPassBuilder {
public:
    static constexpr int MAX_SUBPASSES = 1;
    static constexpr int MAX_ATTACHMENTS = 4;
    static constexpr int MAX_ATTACHMENT_REFERENCES = 4;
    static constexpr int MAX_DEPENDENCIES = 4;

    RenderPassBuilder() {
        reset();
    }

    VkRenderPass create(Renderer& renderer);
    void reset();

    uint32_t addAttachment(VkFormat format,
                           VkAttachmentLoadOp loadOp,
                           VkAttachmentStoreOp storeOp,
                           VkImageLayout initialLayout,
                           VkImageLayout finalLayout,
                           int samples = 1);
    uint32_t addSubpass();
    void addSubpassColorAttachment(uint32_t subpass, uint32_t att, VkImageLayout layout);
    void setSubpassDepthStencilAttachment(uint32_t subpass, uint32_t att, VkImageLayout layout);
    void addDependency(const VkSubpassDependency& d);
private:
    VkRenderPassCreateInfo  ci;
    VkSubpassDescription    subpasses[MAX_SUBPASSES];
    VkAttachmentDescription attachments[MAX_ATTACHMENTS];
    VkAttachmentReference   depthRefs[MAX_SUBPASSES];
    VkAttachmentReference   colorRefs[MAX_SUBPASSES][MAX_ATTACHMENT_REFERENCES];
    VkSubpassDependency     dependencies[MAX_DEPENDENCIES];
};
