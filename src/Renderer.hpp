/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkInstance, VkDevice + related boilerplate abstraction.
///
/// This class may contain code snippets from https://vulkan-tutorial.com/, which
/// has its code listings licensed using CC0 1.0 Universal (public domain).
///
#pragma once
#include <vector>
#include <memory>
#include <string>
#include <cstdint>
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include "GfxSettings.hpp"
#include "Swapchain.hpp"

class Renderer {
    static const uint32_t MAX_TIMESTAMP_QUERY_COUNT = 2;
    friend Swapchain;
public:
    Renderer(const char* appName, const GfxSettings& settings);
    ~Renderer();

    /// Copying not allowed.
    Renderer(const Renderer&) = delete;

    /// Moving not allowed.
    Renderer(Renderer&&) = delete; 
    
    void recordOneTime(std::function<void(VkCommandBuffer)> const& recordFunction);
    void waitForDevice();

    Swapchain&       getSwapchain()                    { return *swapchain;             }
    VkInstance       getInstance()               const { return instance;               }
    VkDevice         getDevice()                 const { return device;                 }
    VkPhysicalDevice getPhysDevice()             const { return physDevice;             }
    VmaAllocator     getAllocator()              const { return allocator;              }
    SDL_Window*      getWindow()                 const { return window;                 }
    uint32_t         getQueueFamily()            const { return gfxQueueFamily;         }
    VkQueue          getQueue()                  const { return gfxQueue;               }
    VkFormat         getBestDepthFormat()        const { return bestDepthFormat;        }
    VkFormat         getBestDepthStencilFormat() const { return bestDepthStencilFormat; }
private:
    void createWindow();
    void fetchNeededExtensions();
    void checkExtensionAvailability();
    void createInstance();
    void createSurface();

    void selectPhysicalDevice();
    bool checkPhysicalDevice(VkPhysicalDevice pd);
    bool checkPhysicalDeviceExtensionSupport(VkPhysicalDevice pd);
    bool checkPhysicalDeviceFeatures(VkPhysicalDevice pd);
    void createDevice();
    void createQueryPool();

    void createAllocator();
    void createCommandPool();
    void updateSurfaceDimensions();

    void selectDepthFormat();

    GfxSettings settings;
    std::string appName;
    SDL_Window* window;
    
    // Device initialization
    VkInstance       instance;
    VkPhysicalDevice physDevice;
    VkDevice         device;
    uint32_t         gfxQueueFamily;
    uint32_t         presentQueueFamily;
    VkQueue          gfxQueue;
    VkQueue          presentQueue;
    VkSurfaceKHR     surface;
    int              surfaceWidth, surfaceHeight;
    VkCommandPool    commandPool;
    VkQueryPool      queryPool;
    VmaAllocator     allocator;
    bool             supportsTimestamps;
    uint32_t         timestampValidBits;
    uint64_t         timestampMask;

    VkFormat bestDepthFormat;
    VkFormat bestDepthStencilFormat;

    VkPhysicalDeviceProperties deviceProperties;

    std::unique_ptr<Swapchain> swapchain;

    std::vector<const char*> extensions;
    std::vector<const char*> deviceExtensions;
};
