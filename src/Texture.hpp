/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkImage/VkImageView abstractions.
///
#pragma once

#include <memory>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

enum eTextureUsage {
    eTextureUsage_Texture,
    eTextureUsage_Depth,
    eTextureUsage_DepthStencil,
    eTextureUsage_RenderTarget,
    eTextureUsageCount,
};
class Renderer; // Forward declaration
class Texture {
public:
    ~Texture();

    /// Allocates and creates a new texture with the specified parameters.
    /// If image view type is a 3D texture, arrayLayers counts as depth.
    Texture(VkDevice device,
            VmaAllocator allocator,
            eTextureUsage usage,
            VkImageViewType type,
            VkFormat format,
            VkImageLayout layout,
            uint32_t width,
            uint32_t height,
            uint32_t arrayLayers = 1,
            uint32_t mipmapLevels = 1);

    /// Takes in a VkImage from the swapchain.
    /// Assumes eTextureUsage_RenderTarget.
    Texture(VkDevice device,
            VkImage image,
            VkFormat format,
            VkImageLayout layout,
            uint32_t width,
            uint32_t height);

    Texture(const Texture&) = delete; // No copying.
    Texture(Texture&& orig);          // Move constructor

    VkImage     getImage()  const { return image;       };
    VkImageView getView()   const { return view;        };
    VkFormat    getFormat() const { return format;      };
    uint32_t    getWidth()  const { return width;       };
    uint32_t    getHeight() const { return height;      };
    uint32_t    getLayers() const { return arrayLayers; };

    /// Records commands to copy data from the specified buffer.
    void copyFromBuffer(VkCommandBuffer cmd,
                        VkBuffer buffer,
                        uint32_t offset,
                        int x,
                        int y,
                        int z,
                        uint32_t w,
                        uint32_t h,
                        uint32_t d = 1);

    /// Records commands to clear the image.
    void clear(VkCommandBuffer cmd, VkClearValue clearValue);

    /// Records pipeline barriers to transition from one image layout
    /// to another. Should be used for copying image data from buffers.
    void transitionLayout(VkCommandBuffer cmd, VkImageLayout oldLayout, VkImageLayout newLayout);

    VkImageAspectFlags getImageAspectFlags();
protected:
    VkDevice        device;
    VmaAllocator    allocator;
    eTextureUsage   usage;
    VkImageViewType type;
    VkFormat        format;
    VkImageLayout   layout;
    VkImage         image;
    VkImageView     view;
    VmaAllocation   allocation;
    uint32_t width, height, arrayLayers, mipmapLevels;
};

class Texture2D : public Texture {
public:
    Texture2D(Renderer& renderer, VkFormat format, uint32_t width, uint32_t height);
};

class RenderTexture2D : public Texture {
public:
    RenderTexture2D(Renderer& renderer, VkFormat format, uint32_t width, uint32_t height);
};

class TextureCubeShadowMap : public Texture {
public:
    TextureCubeShadowMap(Renderer& renderer, VkRenderPass renderPass, uint32_t pxSize);
    ~TextureCubeShadowMap();

    /// No copying.
    TextureCubeShadowMap(const TextureCubeShadowMap&) = delete;

    /// Move constructor.
    TextureCubeShadowMap(TextureCubeShadowMap&&);

    VkFramebuffer getFramebuffer(uint32_t faceID) const { return framebuffers[faceID]; }
    VkImageView   getFaceView(uint32_t faceID)    const { return faceViews[faceID];    }
private:
    VkFramebuffer framebuffers[6];
    VkImageView   faceViews[6];
};