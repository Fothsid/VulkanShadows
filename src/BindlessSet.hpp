/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Class for managing "bindless" VkImageView descriptors.
///
#pragma once

#include "Renderer.hpp"

class BindlessSet {
public:
    static constexpr uint32_t SAMPLERS_BINDING       = 0;
    static constexpr uint32_t IMAGEVIEW_BINDING      = 1;

    // NOTE: Should be increased if using multiple frames in flight.
    static constexpr uint32_t IMAGEVIEW_BUFFER_COUNT = 1;
    
    BindlessSet(Renderer& renderer, uint32_t maxImageViews = 16384, uint32_t maxSamplers = 32);
    ~BindlessSet();
    
    /// Copying not allowed.
    BindlessSet(const BindlessSet&) = delete;

    /// Move constructor 
    BindlessSet(BindlessSet&&);

    /// Bump-allocates an image view index via an internal counter.
    /// Returns the allocated index.
    uint32_t addImageView(VkImageView imageView);

    /// Resets the internal image view counter.
    void clearImageViews();

    /// Returns the current image view count.
    uint32_t getImageViewCount();

    /// Returns the image view index that will be allocated next.
    uint32_t getNextImageViewIndex();

    /// Sets an image view with the specified index.
    void setImageViewIndex(uint32_t index, VkImageView imageView);

    /// Sets a sampler with the specified index.
    void setSamplerIndex(uint32_t index, VkSampler sampler);

    VkDescriptorSetLayout getLayout() const { return layout; }
    VkDescriptorSet getSet() const { return set; }
private:
    Renderer&             renderer;
    VkDescriptorPool      pool;
    VkDescriptorSetLayout layout;
    VkDescriptorSet       set;
    uint32_t              maxImageViews;
    uint32_t              maxSamplers;
    uint32_t              imageViewCounter;
    uint32_t              bufferIndex;
};