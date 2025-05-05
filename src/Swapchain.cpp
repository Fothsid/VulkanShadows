/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkSwapchain + related boilerplate abstraction.
///
/// This class may contain code snippets from https://vulkan-tutorial.com/, which
/// has its code listings licensed using CC0 1.0 Universal (public domain).
///
#include <iostream>
#include "glm.hpp"
#include "Common.hpp"
#include "Swapchain.hpp"
#include "RenderPassBuilder.hpp"

Swapchain::Swapchain(Renderer& renderer)
    : renderer(renderer)
    , acquireAttemptCounter(0)
    , outdated(false)
    , swapchain(nullptr)
{
    fetchCaps();
    createSwapchain();
    createRenderPass();
    createFramebuffers();
    createCommandBuffers();
    createSyncObjects();
}

Swapchain::~Swapchain() {
    destroySyncObjects();
    destroyFramebuffers();
    destroyRenderPass();
    destroySwapchain();
}

void Swapchain::fetchCaps() {
    VkResult r;

    VKCHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(renderer.physDevice, renderer.surface, &surfaceCaps));

    uint32_t formatCount = 0;
    uint32_t modeCount = 0;

    VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physDevice, renderer.surface, &formatCount, nullptr));

    availableSurfaceFormats.resize(formatCount);
    VKCHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(renderer.physDevice,
                                                 renderer.surface,
                                                 &formatCount,
                                                 availableSurfaceFormats.data()));
    VKCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer.physDevice, renderer.surface, &modeCount, nullptr));

    availablePresentModes.resize(modeCount);
    VKCHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(renderer.physDevice,
                                                      renderer.surface,
                                                      &modeCount,
                                                      availablePresentModes.data()));
}

void Swapchain::recreate() {
    vkDeviceWaitIdle(renderer.device);
    destroyFramebuffers();
    fetchCaps();
    createSwapchain();
    createFramebuffers();
    outdated = false;
}

void Swapchain::destroySwapchain() {
    if (renderer.device && swapchain) {
        vkDestroySwapchainKHR(renderer.device, swapchain, nullptr);
    }
}

void Swapchain::destroyRenderPass() {
    if (renderer.device && renderPass) {
        vkDestroyRenderPass(renderer.device, renderPass, nullptr);
    }
}

void Swapchain::destroyFramebuffers() {
    if (renderer.device) {
        for (auto framebuffer : framebuffers) {
            vkDestroyFramebuffer(renderer.device, framebuffer, nullptr);
        }
        depthBuffer = nullptr;
        textures.clear();
    }
}

void Swapchain::destroySyncObjects() {
    if (renderer.device) {
        vkDestroySemaphore(renderer.device, renderFinishedSema, nullptr);
        vkDestroySemaphore(renderer.device, imageAvailableSema, nullptr);
        vkDestroyFence(renderer.device, renderFence, nullptr);
    }
}

void Swapchain::createSwapchain() {
    if (availableSurfaceFormats.size() == 0 || availablePresentModes.size() == 0) {
        throw std::runtime_error("There isn't a single surface format/presentation mode available.");
    }

    presentMode   = selectPresentMode();
    surfaceFormat = selectSurfaceFormat();
    extent        = selectExtent();

    uint32_t wantImages = surfaceCaps.minImageCount + 1;
    if (surfaceCaps.maxImageCount != 0 && wantImages > surfaceCaps.maxImageCount)
        wantImages = surfaceCaps.maxImageCount;

    VkSwapchainCreateInfoKHR sci = {};
    sci.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    sci.surface          = renderer.surface;
    sci.minImageCount    = wantImages;
    sci.imageFormat      = surfaceFormat.format;
    sci.imageColorSpace  = surfaceFormat.colorSpace;
    sci.imageExtent      = extent;
    sci.imageArrayLayers = 1;
    sci.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    sci.preTransform     = surfaceCaps.currentTransform;
    sci.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    sci.presentMode      = presentMode;
    sci.clipped          = VK_TRUE;
    sci.oldSwapchain     = swapchain;

    VKCHECK(vkCreateSwapchainKHR(renderer.device, &sci, nullptr, &swapchain));
    if (sci.oldSwapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(renderer.device, sci.oldSwapchain, nullptr);
    }
}

void Swapchain::createRenderPass() {

    constexpr VkSubpassDependency dependencyA = {
        .srcSubpass      = VK_SUBPASS_EXTERNAL,
        .dstSubpass      = 0,
        .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT
                         | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };
    constexpr VkSubpassDependency dependencyB = {
        .srcSubpass      = 0,
        .dstSubpass      = VK_SUBPASS_EXTERNAL,
        .srcStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask    = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
                         | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,
        .dstAccessMask   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    };

    RenderPassBuilder builder;
    builder.addAttachment(surfaceFormat.format,
                          VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                          VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    builder.addAttachment(renderer.bestDepthStencilFormat,
                          VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    builder.addSubpass();
    builder.addSubpassColorAttachment(0, 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    builder.setSubpassDepthStencilAttachment(0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    builder.addDependency(dependencyA);
    builder.addDependency(dependencyB);
    renderPass = builder.create(renderer);
}

void Swapchain::createFramebuffers() {
    uint32_t swapchainImageCount;
    vkGetSwapchainImagesKHR(renderer.device, swapchain, &swapchainImageCount, nullptr);
    std::vector<VkImage> images(swapchainImageCount);
    vkGetSwapchainImagesKHR(renderer.device, swapchain, &swapchainImageCount, images.data());

    for (uint32_t i = 0; i < swapchainImageCount; ++i) {
        textures.emplace_back(
                renderer.device,
                images[i],
                surfaceFormat.format,
                VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                extent.width,
                extent.height
            );
    }

    depthBuffer = std::make_unique<Texture>(
        renderer.device,
        renderer.allocator,
        eTextureUsage_DepthStencil,
        VK_IMAGE_VIEW_TYPE_2D,
        renderer.bestDepthStencilFormat,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        extent.width,
        extent.height
    );

    framebuffers.resize(textures.size());
    for (int i = 0; i < framebuffers.size(); ++i) {
        VkImageView views[] = {
            textures[i].getView(),
            depthBuffer->getView()
        };

        VkFramebufferCreateInfo fci = { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
        fci.renderPass = renderPass;
        fci.attachmentCount = 2;
        fci.pAttachments = views;
        fci.width = extent.width;
        fci.height = extent.height;
        fci.layers = 1;

        VKCHECK(vkCreateFramebuffer(renderer.device, &fci, nullptr, &framebuffers[i]));
    }

    // Initialize texture memory so that validation layers won't complain
    // about image layout being undefined.
    renderer.recordOneTime([&](VkCommandBuffer cmdbuf){
        for (uint32_t i = 0; i < swapchainImageCount; ++i) {
            textures[i].clear(cmdbuf, {});
        }
        depthBuffer->clear(cmdbuf, {});
    });
}

void Swapchain::createCommandBuffers() {
    VkCommandBufferAllocateInfo cbai = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
    cbai.commandPool        = renderer.commandPool;
    cbai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;

    VKCHECK(vkAllocateCommandBuffers(renderer.device, &cbai, &commandBuffer));
}

void Swapchain::createSyncObjects() {
    VkSemaphoreCreateInfo sci = { VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    VkFenceCreateInfo fci = { VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    fci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VKCHECK(vkCreateSemaphore(renderer.device, &sci, nullptr, &imageAvailableSema));
    VKCHECK(vkCreateSemaphore(renderer.device, &sci, nullptr, &renderFinishedSema));
    VKCHECK(vkCreateFence(renderer.device, &fci, nullptr, &renderFence));
}

VkPresentModeKHR Swapchain::selectPresentMode() {
    VkPresentModeKHR preferredMode = VK_PRESENT_MODE_FIFO_KHR;
    if (!renderer.settings.vsync) {
        preferredMode = VK_PRESENT_MODE_MAILBOX_KHR; // VSync off
    }

    for (const auto& mode : availablePresentModes) {
        if (mode == preferredMode) {
            return mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR; // FIFO is always available.
}

struct SurfaceFormatPriority {
    VkFormat format;
    int priority;
};

static const SurfaceFormatPriority surface_format_priorities[][2] = {
    {
        { VK_FORMAT_B8G8R8A8_UNORM,  100 },
        { VK_FORMAT_B8G8R8A8_SRGB,   50  },
    },
    {
        { VK_FORMAT_B8G8R8A8_SRGB,   100 },
        { VK_FORMAT_B8G8R8A8_UNORM,  50  },
    },
};

VkSurfaceFormatKHR Swapchain::selectSurfaceFormat() {
    int prio = -1;
    int selected = 0;
    int i = 0;
    for (const auto& format : availableSurfaceFormats) {
        for (const auto& p : surface_format_priorities[renderer.settings.srgbColor&1]) {
            if (p.format == format.format && p.priority > prio) {
                prio     = p.priority;
                selected = i;
            }
        }
        i++;
    }
    return availableSurfaceFormats[selected];
}

VkExtent2D Swapchain::selectExtent() {
    renderer.updateSurfaceDimensions();
    uint32_t width = uint32_t(renderer.surfaceWidth);
    uint32_t height = uint32_t(renderer.surfaceHeight);

    width = glm::clamp(width, surfaceCaps.minImageExtent.width, surfaceCaps.maxImageExtent.width);
    height = glm::clamp(height, surfaceCaps.minImageExtent.height, surfaceCaps.maxImageExtent.height);

    return { width, height };
}

void Swapchain::recordFrame(std::function<void(Swapchain&, VkCommandBuffer)> const& recordFunction) {
    // Wait for the previous frame to finish.
    vkWaitForFences(renderer.device, 1, &renderFence, VK_TRUE, UINT64_MAX);

    acquireAttemptCounter = 0;
    VkResult r;
    do {
        r = vkAcquireNextImageKHR(renderer.device, swapchain, UINT64_MAX,
                                  imageAvailableSema, VK_NULL_HANDLE, &imageIndex);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            if (acquireAttemptCounter++ < 2) {
                recreate();
            } else {
                throw std::runtime_error("Couldn't acquire an image multiple times in a row after recreation :(");
            }
        } 
    } while (r == VK_ERROR_OUT_OF_DATE_KHR);
    
    if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error(std::format("Couldn't acquire an image ({})", string_VkResult(r)));
    }
    currentFramebuffer = framebuffers[imageIndex];

    VkCommandBufferBeginInfo cbbi = { VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
    vkResetCommandBuffer(commandBuffer, 0);
    vkBeginCommandBuffer(commandBuffer, &cbbi);

    // Record timestamps if needed.
    if (renderer.settings.needTimestamps) {
        vkCmdResetQueryPool(commandBuffer, renderer.queryPool, 0, 2);
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, renderer.queryPool, 0);
    }
    recordFunction(*this, commandBuffer);
    if (renderer.settings.needTimestamps) {
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, renderer.queryPool, 1);
    }
    vkEndCommandBuffer(commandBuffer);

    vkResetFences(renderer.device, 1, &renderFence);
    static constexpr VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    
    VkSubmitInfo si = { VK_STRUCTURE_TYPE_SUBMIT_INFO };
    si.waitSemaphoreCount   = 1;
    si.signalSemaphoreCount = 1;
    si.commandBufferCount   = 1;
    si.pWaitSemaphores      = &imageAvailableSema;
    si.pSignalSemaphores    = &renderFinishedSema;
    si.pWaitDstStageMask    = waitStages;
    si.pCommandBuffers      = &commandBuffer;

    VKCHECK(vkQueueSubmit(renderer.gfxQueue, 1, &si, renderFence));

    VkPresentInfoKHR pi = { VK_STRUCTURE_TYPE_PRESENT_INFO_KHR };
    pi.waitSemaphoreCount = 1;
    pi.swapchainCount     = 1;
    pi.pWaitSemaphores    = &renderFinishedSema;
    pi.pSwapchains        = &swapchain;
    pi.pImageIndices      = &imageIndex;
    r = vkQueuePresentKHR(renderer.presentQueue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || outdated) {
        recreate();
    } else if (r != VK_SUCCESS) {
        throw std::runtime_error("Couldn't present a swapchain frame.");
    }

    if (renderer.settings.needTimestamps) {
        // Wait for the frame to finish rendering early to take render time.
        // (we care about how long individual frames take to render in
        //  this case, we're not trying to maximize the FPS or anything).
        // Also, as it turns out, VK_QUERY_RESULT_WAIT_BIT does not do the thing
        // I expected it to do. Waiting on the render fence is required, otherwise
        // the timestamp values can be undefined.
        vkWaitForFences(renderer.device, 1, &renderFence, VK_TRUE, UINT64_MAX);
        VKCHECK(vkGetQueryPoolResults(renderer.device, renderer.queryPool, 0, ARRAY_COUNT(timestamps), sizeof(timestamps),
                                      timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT));
        timestamps[0] &= renderer.timestampMask;
        timestamps[1] &= renderer.timestampMask;
        float delta = glm::abs(float(timestamps[1]) - float(timestamps[0]));
        float milliseconds = delta * renderer.deviceProperties.limits.timestampPeriod / 1000000.0f;
        std::cout << milliseconds << "\n";
    }
}

void Swapchain::beginRenderPass() {
    VkClearValue clearValues[2];
    clearValues[0].color        = {{ 0, 0, 0, 0 }};
    clearValues[1].depthStencil = { 1, 0 };

    VkRenderPassBeginInfo brp = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    brp.renderPass        = renderPass;
    brp.framebuffer       = currentFramebuffer;
    brp.renderArea.offset = {0, 0};
    brp.renderArea.extent = extent;
    brp.clearValueCount   = 2;
    brp.pClearValues      = clearValues;
    vkCmdBeginRenderPass(commandBuffer, &brp, VK_SUBPASS_CONTENTS_INLINE);
}

void Swapchain::setDefaultViewportScissor() {
    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = extent.width;
    viewport.height   = extent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}