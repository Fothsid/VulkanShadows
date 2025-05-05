/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkSwapchain + related boilerplate abstraction.
///
/// This class may contain code snippets from https://vulkan-tutorial.com/, which
/// has its code listings licensed using CC0 1.0 Universal (public domain).
///
#pragma once
#include <functional>
#include "Texture.hpp"
#include "glm.hpp"

class Renderer; // Forward declaration
class Swapchain {
    friend Renderer;
public:
    Swapchain(Renderer& renderer);
    ~Swapchain();

    /// Copying is not allowed.
    Swapchain(const Swapchain&) = delete; 

    /// Moving is not allowed.
    Swapchain(const Swapchain&&) = delete;

    void recordFrame(std::function<void(Swapchain&, VkCommandBuffer)> const& recordFunction);
    void beginRenderPass();
    void setDefaultViewportScissor();

    uint32_t         getImageCount()   const { return textures.size(); }
    uint32_t         getCurrentImage() const { return imageIndex; } 
    VkRenderPass     getRenderPass()   const { return renderPass; }
    Texture&         getDepthBuffer()  const { return *depthBuffer; }
    uint32_t         getWidth()        const { return extent.width; }
    uint32_t         getHeight()       const { return extent.height; }
    VkExtent2D       getExtent()       const { return extent; }
    VkFormat         getFormat()       const { return surfaceFormat.format; }
    VkPresentModeKHR getPresentMode()  const { return presentMode; }

    void markAsOutdated() { outdated = true; };
private:
    void recreate();
    void fetchCaps();
    void createSwapchain();
    void createRenderPass();
    void createFramebuffers();
    void createCommandBuffers();
    void createSyncObjects();

    // NOTE: Command buffers are implicitly destroyed when the command pool
    // is destroyed, and that one is owned by the Renderer.
    void destroySwapchain();
    void destroyRenderPass();
    void destroyFramebuffers();
    void destroySyncObjects();

    VkPresentModeKHR   selectPresentMode();
    VkSurfaceFormatKHR selectSurfaceFormat();
    VkExtent2D         selectExtent();

    Renderer&                  renderer;
    VkSwapchainKHR             swapchain;
    VkExtent2D                 extent;
    VkSurfaceFormatKHR         surfaceFormat;
    VkPresentModeKHR           presentMode;
    std::vector<Texture>       textures;
    std::unique_ptr<Texture>   depthBuffer;
    std::vector<VkFramebuffer> framebuffers;
    VkRenderPass               renderPass;
    VkCommandBuffer            commandBuffer;
    VkSemaphore                imageAvailableSema;
    VkSemaphore                renderFinishedSema;
    VkFence                    renderFence;
    uint32_t                   imageIndex;
    VkFramebuffer              currentFramebuffer;
    uint64_t                   timestamps[2];

    std::vector<VkPresentModeKHR>   availablePresentModes;
    std::vector<VkSurfaceFormatKHR> availableSurfaceFormats;
    VkSurfaceCapabilitiesKHR        surfaceCaps;
    
    bool outdated;
    int  acquireAttemptCounter;
};