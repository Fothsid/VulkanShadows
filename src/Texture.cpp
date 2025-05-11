/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkImage/VkImageView abstractions.
///
#include "Common.hpp"
#include "Texture.hpp"
#include "Renderer.hpp"

SwapchainTexture::SwapchainTexture(VkDevice device,
                                   VkRenderPass renderPass,
                                   VkImage image,
                                   VkFormat format,
                                   uint32_t width,
                                   uint32_t height,
                                   VkImageView depthView)
    : Texture(device,
              image,
              eTextureUsage_RenderTarget,
              VK_IMAGE_VIEW_TYPE_2D,
              format,
              VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
              width,
              height)
{
    VkImageView views[] = {
        view,
        depthView
    };

    VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fci.renderPass      = renderPass;
    fci.width           = width;
    fci.height          = height;
    fci.layers          = 1;
    fci.attachmentCount = 2;
    fci.pAttachments    = views;
    VKCHECK(vkCreateFramebuffer(device, &fci, nullptr, &framebuffer));
}

SwapchainTexture::~SwapchainTexture() {
    vkDestroyFramebuffer(device, framebuffer, nullptr);
    framebuffer = VK_NULL_HANDLE;
}

SwapchainTexture::SwapchainTexture(SwapchainTexture&& o)
    : Texture(std::move(o))
{
    framebuffer   = o.framebuffer;
    o.framebuffer = VK_NULL_HANDLE;
}

Texture2D::Texture2D(Renderer& renderer, VkFormat format, uint32_t width, uint32_t height)
    : Texture(renderer.getDevice(),
              renderer.getAllocator(),
              eTextureUsage_Texture,
              VK_IMAGE_VIEW_TYPE_2D,
              format,
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              width,
              height)
{}

TextureCubeShadowMap::TextureCubeShadowMap(Renderer& renderer, VkRenderPass renderPass, uint32_t pxSize)
    : Texture(renderer.getDevice(),
              renderer.getAllocator(),
              eTextureUsage_Depth,
              VK_IMAGE_VIEW_TYPE_CUBE,
              renderer.getBestDepthFormat(),
              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
              pxSize, pxSize, 6)
{
    VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    vci.image            = image;
    vci.format           = format;
    vci.components       = { VK_COMPONENT_SWIZZLE_R };
    vci.subresourceRange = { VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 };
    for (uint32_t i = 0; i < 6; ++i) {
        vci.subresourceRange.baseArrayLayer = i;
        VKCHECK(vkCreateImageView(device, &vci, nullptr, &faceViews[i]));
    }

    VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    fci.renderPass      = renderPass;
    fci.attachmentCount = 1;
    fci.width           = pxSize;
    fci.height          = pxSize;
    fci.layers          = 1;
    for (uint32_t i = 0; i < 6; ++i) {
        fci.pAttachments = &faceViews[i];
        VKCHECK(vkCreateFramebuffer(device, &fci, nullptr, &framebuffers[i]));
    }
}

TextureCubeShadowMap::~TextureCubeShadowMap() {
    for (auto& v : framebuffers) {
        vkDestroyFramebuffer(device, v, nullptr);
        v = VK_NULL_HANDLE;
    }
    for (auto& v : faceViews) {
        vkDestroyImageView(device, v, nullptr);
        v = VK_NULL_HANDLE;
    }
}

TextureCubeShadowMap::TextureCubeShadowMap(TextureCubeShadowMap&& o)
    : Texture(std::move(o))
{
    memcpy(framebuffers, o.framebuffers, sizeof(framebuffers));
    memcpy(faceViews, o.faceViews, sizeof(faceViews));
    memset(o.framebuffers, 0, sizeof(o.framebuffers));
    memset(o.faceViews, 0, sizeof(o.faceViews));
}

Texture::~Texture() {
    if (view != VK_NULL_HANDLE) {
        vkDestroyImageView(device, view, nullptr);
        view = VK_NULL_HANDLE;
    }

    if (allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(allocator, image, allocation);
        image = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

Texture::Texture(VkDevice device,
                 VmaAllocator allocator,
                 eTextureUsage usage,
                 VkImageViewType type,
                 VkFormat format,
                 VkImageLayout layout,
                 uint32_t width,
                 uint32_t height,
                 uint32_t arrayLayers,
                 uint32_t mipmapLevels)
    : device(device)
    , allocator(allocator)
    , usage(usage)
    , type(type)
    , format(format)
    , layout(layout)
    , image(VK_NULL_HANDLE)
    , view(VK_NULL_HANDLE)
    , allocation(VK_NULL_HANDLE)
    , width(width), height(height)
    , arrayLayers(arrayLayers)
    , mipmapLevels(mipmapLevels)
{
    VkImageCreateInfo ici = { VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO };
    ici.format        = format;
    ici.extent        = {width, height, 1};
    ici.mipLevels     = mipmapLevels;
    ici.arrayLayers   = arrayLayers;
    ici.samples       = VK_SAMPLE_COUNT_1_BIT;
    ici.tiling        = VK_IMAGE_TILING_OPTIMAL;
    ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ici.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;

    VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.viewType   = type;
    vci.format     = format;
    vci.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    vci.subresourceRange = { getImageAspectFlags(), 0, mipmapLevels, 0, arrayLayers };

    switch (type) {
    case VK_IMAGE_VIEW_TYPE_1D:         ici.imageType = VK_IMAGE_TYPE_1D; break;
    case VK_IMAGE_VIEW_TYPE_2D:         ici.imageType = VK_IMAGE_TYPE_2D; break;
    case VK_IMAGE_VIEW_TYPE_1D_ARRAY:   ici.imageType = VK_IMAGE_TYPE_1D; break;
    case VK_IMAGE_VIEW_TYPE_2D_ARRAY:   ici.imageType = VK_IMAGE_TYPE_2D; break;
    case VK_IMAGE_VIEW_TYPE_CUBE:
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    case VK_IMAGE_VIEW_TYPE_CUBE_ARRAY:
        ici.imageType = VK_IMAGE_TYPE_2D;
        ici.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
        break;
    case VK_IMAGE_VIEW_TYPE_3D:
        ici.imageType = VK_IMAGE_TYPE_3D;
        ici.extent = {width, height, arrayLayers};
        break;
    }

    switch (usage) {
    case eTextureUsage_Texture:
        ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                  | VK_IMAGE_USAGE_SAMPLED_BIT;
        break;
    case eTextureUsage_Depth:
        ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                  | VK_IMAGE_USAGE_SAMPLED_BIT
                  | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    case eTextureUsage_DepthStencil:
        ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                  | VK_IMAGE_USAGE_SAMPLED_BIT
                  | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
        break;
    case eTextureUsage_RenderTarget:
        ici.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                  | VK_IMAGE_USAGE_TRANSFER_DST_BIT
                  | VK_IMAGE_USAGE_SAMPLED_BIT
                  | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        break;
    }

    VmaAllocationCreateInfo aci = {};
    aci.usage         = VMA_MEMORY_USAGE_GPU_ONLY;
    aci.flags         = VMA_ALLOCATION_CREATE_WITHIN_BUDGET_BIT;
    aci.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    VKCHECK(vmaCreateImage(allocator, &ici, &aci, &image, &allocation, nullptr));

    vci.image = image;
    VKCHECK(vkCreateImageView(device, &vci, nullptr, &view));
}

Texture::Texture(VkDevice device,
                 VkImage image,
                 eTextureUsage usage,
                 VkImageViewType type,
                 VkFormat format,
                 VkImageLayout layout,
                 uint32_t width,
                 uint32_t height,
                 uint32_t arrayLayers,
                 uint32_t mipmapLevels)
    : device(device)
    , allocator(VK_NULL_HANDLE)
    , usage(usage)
    , type(type)
    , format(format)
    , layout(layout)
    , image(image)
    , view(VK_NULL_HANDLE)
    , allocation(VK_NULL_HANDLE)
    , width(width), height(height)
    , arrayLayers(arrayLayers)
    , mipmapLevels(mipmapLevels)
{
    VkImageViewCreateInfo vci = { VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO };
    vci.viewType   = VK_IMAGE_VIEW_TYPE_2D;
    vci.image      = image;
    vci.format     = format;
    vci.components = {
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY,
        VK_COMPONENT_SWIZZLE_IDENTITY
    };
    vci.subresourceRange = { getImageAspectFlags(), 0, mipmapLevels, 0, arrayLayers };

    VKCHECK(vkCreateImageView(device, &vci, nullptr, &view));
}

Texture::Texture(Texture&& orig)
    : device(orig.device)
    , allocator(orig.allocator)
    , usage(orig.usage)
    , type(orig.type)
    , format(orig.format)
    , layout(orig.layout)
    , image(orig.image)
    , view(orig.view)
    , allocation(orig.allocation)
    , width(orig.width), height(orig.height)
    , arrayLayers(orig.arrayLayers)
    , mipmapLevels(orig.mipmapLevels)
{
    orig.view = VK_NULL_HANDLE;
    orig.allocation = VK_NULL_HANDLE;
    orig.image = VK_NULL_HANDLE;
}

void Texture::copyFromBuffer(VkCommandBuffer cmd, VkBuffer buffer, uint32_t offset, int x, int y, int z, uint32_t w, uint32_t h, uint32_t d) {
    transitionLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy region = {};
    region.bufferOffset = offset;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;

    region.imageSubresource.aspectMask = getImageAspectFlags();
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = type == VK_IMAGE_VIEW_TYPE_3D ? 0 : z;
    region.imageSubresource.layerCount = type == VK_IMAGE_VIEW_TYPE_3D ? 1 : arrayLayers;

    region.imageOffset = { x, y, type == VK_IMAGE_VIEW_TYPE_3D ? z : 0 };
    region.imageExtent = { w, h, type == VK_IMAGE_VIEW_TYPE_3D ? d : 1 };

    vkCmdCopyBufferToImage(
        cmd,
        buffer,
        image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &region
    );

    transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);
}

void Texture::clear(VkCommandBuffer cmd, VkClearValue clearValue) {
    transitionLayout(cmd, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkImageSubresourceRange range = {
        .aspectMask     = getImageAspectFlags(),
        .baseMipLevel   = 0,
        .levelCount     = mipmapLevels,
        .baseArrayLayer = 0,
        .layerCount     = arrayLayers
    };
    if (usage == eTextureUsage_Depth || usage == eTextureUsage_DepthStencil) {
        vkCmdClearDepthStencilImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.depthStencil, 1, &range);
    } else {
        vkCmdClearColorImage(cmd, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearValue.color, 1, &range);
    }
    transitionLayout(cmd, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, layout);
}

void Texture::transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkImageMemoryBarrier barrier = { VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
    barrier.oldLayout           = oldLayout;
    barrier.newLayout           = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image               = image;
    barrier.subresourceRange.aspectMask     = getImageAspectFlags();
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = mipmapLevels;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = arrayLayers;
    barrier.srcAccessMask                   = 0;
    barrier.dstAccessMask                   = 0;

    // Try to guess the stage and access masks based on from/to which layouts
    // we're trying to transition. Probably not effective as inserting manually
    // configured barriers, but should be safe. Plus, in this project we don't
    // really need to transition layouts outside of the initial image load
    // anyway.

    VkPipelineStageFlags srcStageMask = 0;
    VkPipelineStageFlags dstStageMask = 0;
    switch (oldLayout) {
    default:
        throw std::runtime_error("Unknown old layout for image layout transition.");
        return;
    case VK_IMAGE_LAYOUT_UNDEFINED:
        barrier.srcAccessMask = 0;
        srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                              | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                              | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        srcStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                     | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    }

    switch (newLayout) {
    default:
        throw std::runtime_error("Unknown new layout for image layout transition.");
        return;
    case VK_IMAGE_LAYOUT_UNDEFINED:
        barrier.srcAccessMask = 0;
        srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        srcStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                              | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
                              | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
                     | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        break;
    }

    vkCmdPipelineBarrier(
        cmd,
        srcStageMask, dstStageMask,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );
}

VkImageAspectFlags Texture::getImageAspectFlags() {
    switch (usage) {
    case eTextureUsage_Texture:
    case eTextureUsage_RenderTarget:
        return VK_IMAGE_ASPECT_COLOR_BIT;
    case eTextureUsage_Depth:
        return VK_IMAGE_ASPECT_DEPTH_BIT;
    case eTextureUsage_DepthStencil:
        return VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT;
    default: return 0;
    }
}