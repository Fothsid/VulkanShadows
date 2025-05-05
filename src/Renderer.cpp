/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkInstance, VkDevice + related boilerplate abstraction.
///
/// This class may contain code snippets from https://vulkan-tutorial.com/, which
/// has its code listings licensed using CC0 1.0 Universal (public domain).
///
#include <iostream>
#include "glm.hpp"
#include "Common.hpp"
#include "Renderer.hpp"

#define API_VERSION VK_API_VERSION_1_2

// For use with framebuffers that will be used for stencil
// shadow volumes. (swapchain depth buffer)
static const VkFormat depth_stencil_format_preference[] = {
    VK_FORMAT_D32_SFLOAT_S8_UINT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM_S8_UINT,
};

// For use with anything else that doesn't require the stencil buffer.
// (shadow maps)
static const VkFormat depth_format_preference[] = {
    VK_FORMAT_D32_SFLOAT,
    VK_FORMAT_D24_UNORM_S8_UINT,
    VK_FORMAT_D16_UNORM,
    VK_FORMAT_D16_UNORM_S8_UINT,
};

Renderer::Renderer(const char* appName, const GfxSettings& settings)
    : appName(appName)
    , settings(settings)
{
    deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    createWindow();
    fetchNeededExtensions();
    checkExtensionAvailability();

    createInstance();
    createSurface();
    selectPhysicalDevice();
    selectDepthFormat();
    createDevice();
    createQueryPool();
    createAllocator();
    createCommandPool();
    swapchain = std::make_unique<Swapchain>(*this);
}

Renderer::~Renderer() {
    swapchain = nullptr;
    vkDestroyCommandPool(device, commandPool, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyQueryPool(device, queryPool, nullptr);
    vkDestroyDevice(device, nullptr);
    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    SDL_DestroyWindow(window);
}

void Renderer::createWindow() {
    uint32_t flags = SDL_WINDOW_SHOWN | SDL_WINDOW_VULKAN;

    if (settings.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }

    window = SDL_CreateWindow(appName.c_str(),
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              settings.width,
                              settings.height,
                              flags);
    if (!window) {
        throw std::runtime_error("Could not create SDL window.");
    }
}

void Renderer::fetchNeededExtensions() {
    uint32_t extensionCount = 0;
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, nullptr);

    extensions.resize(extensionCount);
    SDL_Vulkan_GetInstanceExtensions(window, &extensionCount, extensions.data());
}

void Renderer::checkExtensionAvailability() {
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, availableExtensions.data());

    for (const char* name : extensions) {
        bool found = false;
        for (const auto& properties : availableExtensions) {
            if (strcmp(name, properties.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            throw std::runtime_error(std::format("Vulkan extension {} is not available.", name));
        }
    }
}

void Renderer::createInstance() {
    VkApplicationInfo app = { VK_STRUCTURE_TYPE_APPLICATION_INFO };
    app.pNext = nullptr;
    app.pApplicationName = appName.c_str();
    app.applicationVersion = 0;
    app.pEngineName = appName.c_str();
    app.engineVersion = 0;
    app.apiVersion = API_VERSION;

    VkInstanceCreateInfo instInfo = { VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO };
    instInfo.pNext = nullptr;
    instInfo.pApplicationInfo = &app;
    instInfo.enabledExtensionCount = extensions.size();
    instInfo.ppEnabledExtensionNames = extensions.data();

    VKCHECK(vkCreateInstance(&instInfo, nullptr, &instance));
}

void Renderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(window, instance, &surface)) {
        throw std::runtime_error("SDL failed to create a VkSurfaceKHR");
    }
}

void Renderer::selectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    physDevice = VK_NULL_HANDLE;

    int index = 0;
    for (const auto& pd : devices) {
        vkGetPhysicalDeviceProperties(pd, &deviceProperties);
        if (checkPhysicalDevice(pd) && (settings.gpuIndex < 0 || settings.gpuIndex == index)) {
            std::cerr << std::format("GPU: {}\n", deviceProperties.deviceName);
            physDevice = pd;
            break;
        }
        index++;
    }

    if (physDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Couldn't select a GPU.");
    }
}

void Renderer::selectDepthFormat() {
    bestDepthFormat = VK_FORMAT_UNDEFINED;
    bestDepthStencilFormat = VK_FORMAT_UNDEFINED;

    VkFormatFeatureFlags wantedFlags = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
                                     | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;

    for (auto format : depth_stencil_format_preference) {
        VkFormatProperties prop;
        vkGetPhysicalDeviceFormatProperties(physDevice, format, &prop);
        if ((prop.optimalTilingFeatures & wantedFlags) == wantedFlags) {
            bestDepthStencilFormat = format;
            break;
        }
    }

    for (auto format : depth_format_preference) {
        VkFormatProperties prop;
        vkGetPhysicalDeviceFormatProperties(physDevice, format, &prop);
        if ((prop.optimalTilingFeatures & wantedFlags) == wantedFlags) {
            bestDepthFormat = format;
            break;
        }
    }
    
    if (bestDepthFormat == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("Couldn't select appropriate depth format.");
    }

    if (bestDepthStencilFormat == VK_FORMAT_UNDEFINED) {
        throw std::runtime_error("Couldn't select appropriate depth+stencil format.");
    }
}

bool Renderer::checkPhysicalDevice(VkPhysicalDevice pd) {
    // For now, all we want is for the GPU to have a graphics queue family.
    // Any graphics/compute queue can also handle transfers, so we really just
    // need one.

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pd, &queueFamilyCount, queueFamilies.data());

    presentQueueFamily = -1;
    gfxQueueFamily = -1;

    int index = 0;
    for (const auto& queueFamily : queueFamilies) {
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(pd, index, surface, &presentSupport);
        if (presentQueueFamily == -1 && presentSupport) {
            presentQueueFamily = index;
        }
        if (gfxQueueFamily == -1 &&
            (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
            (!settings.needTimestamps || queueFamily.timestampValidBits != 0)) // Queue should support timestamps in test mode
        {
            timestampValidBits = queueFamily.timestampValidBits;
            timestampMask      = timestampValidBits >= 64 ? ~uint64_t(0) : (uint64_t(1) << timestampValidBits) - 1;
            gfxQueueFamily     = index;
        }
        if (presentQueueFamily != -1 && gfxQueueFamily != -1) {
            break;
        }
        index++;
    }

    if (presentQueueFamily == -1 || gfxQueueFamily == -1) {
        if (presentQueueFamily == -1) {
            std::cerr << "Couldn't select present queue family.\n";
        }
        if (gfxQueueFamily == -1) {
            std::cerr << "Couldn't select graphics queue family.\n";
        } 
        return false;  
    }
    if (!checkPhysicalDeviceExtensionSupport(pd)) {
        return false;
    }
    if (!checkPhysicalDeviceFeatures(pd)) {
        return false;
    }
    return true;
}

bool Renderer::checkPhysicalDeviceExtensionSupport(VkPhysicalDevice pd) {
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(pd, nullptr, &extensionCount, availableExtensions.data());

    for (const char* extension : deviceExtensions) {
        bool found = false;
        for (const auto& properties : availableExtensions) {
            if (strcmp(extension, properties.extensionName) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            std::cerr << std::format("Extension {} not supported by device.\n", extension);
            return false;
        }
    }
    return true;
}

bool Renderer::checkPhysicalDeviceFeatures(VkPhysicalDevice pd) {
    VkPhysicalDeviceVulkan12Features vulkan12Features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceFeatures2 features = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features.pNext = &vulkan12Features;

    vkGetPhysicalDeviceFeatures2(pd, &features);

    #define CHECKFEATUREVK12(f) \
        if (!vulkan12Features.f) {\
            std::cerr << std::format("VkPhysicalDeviceVulkan12Features::" #f " is not supported by {}.\n", deviceProperties.deviceName);\
            return false;\
        }
    #define CHECKFEATURE2(f) \
        if (!features.features.f) {\
            std::cerr << std::format("VkPhysicalDeviceFeatures2::" #f " is not supported by {}.\n", deviceProperties.deviceName);\
            return false;\
        }
    CHECKFEATUREVK12(bufferDeviceAddress);
    CHECKFEATUREVK12(runtimeDescriptorArray);
    CHECKFEATUREVK12(descriptorBindingPartiallyBound);
    CHECKFEATUREVK12(descriptorBindingSampledImageUpdateAfterBind);
    CHECKFEATUREVK12(shaderSampledImageArrayNonUniformIndexing);
    CHECKFEATURE2(geometryShader);
    CHECKFEATURE2(depthClamp);
    #undef CHECKFEATURE2

    if (settings.needTimestamps && deviceProperties.limits.timestampPeriod == 0) {
        std::cerr << std::format("{} does not support timestamp queries.", deviceProperties.deviceName);
        return false;
    }

    return true;
}

void Renderer::createDevice() {
    float queuePriority = 1.0f;
    
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    auto& i = queueCreateInfos.emplace_back();
    i.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    i.queueFamilyIndex = gfxQueueFamily;
    i.queueCount = 1;
    i.pQueuePriorities = &queuePriority;

    // No need to create more queues if both graphics and presentation
    // queues are of the same family.
    if (gfxQueueFamily != presentQueueFamily) {
        auto& i = queueCreateInfos.emplace_back();
        i.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        i.queueFamilyIndex = presentQueueFamily;
        i.queueCount = 1;
        i.pQueuePriorities = &queuePriority;
    }

    // Enabling features for buffer addressing inside shaders
    // and bindless image/sampler workflow.
    VkPhysicalDeviceVulkan12Features vulkan12Features = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES };
    vulkan12Features.bufferDeviceAddress = true;
    vulkan12Features.runtimeDescriptorArray = true;
    vulkan12Features.descriptorBindingPartiallyBound = true;
    vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = true;
    vulkan12Features.shaderSampledImageArrayNonUniformIndexing = true;

    VkPhysicalDeviceFeatures2 deviceFeatures2 = { VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
    deviceFeatures2.pNext = &vulkan12Features;
    deviceFeatures2.features.geometryShader = true;
    deviceFeatures2.features.depthClamp = true;

    VkDeviceCreateInfo createInfo = { VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO };
    createInfo.pNext = &deviceFeatures2;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = queueCreateInfos.size();
    createInfo.enabledExtensionCount = deviceExtensions.size();
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();

    VKCHECK(vkCreateDevice(physDevice, &createInfo, nullptr, &device));
    vkGetDeviceQueue(device, gfxQueueFamily, 0, &gfxQueue);
    vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
}

void Renderer::createQueryPool() {
    if (!settings.needTimestamps) {
        queryPool = VK_NULL_HANDLE;
        return;
    }

    VkQueryPoolCreateInfo qpi = { VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO };
    qpi.queryType  = VK_QUERY_TYPE_TIMESTAMP;
    qpi.queryCount = MAX_TIMESTAMP_QUERY_COUNT;
    VKCHECK(vkCreateQueryPool(device, &qpi, nullptr, &queryPool));
}

void Renderer::createAllocator() {
    VmaVulkanFunctions vf = {};
    vf.vkGetInstanceProcAddr = &vkGetInstanceProcAddr;
    vf.vkGetDeviceProcAddr = &vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo ci = {};
    ci.flags = VMA_ALLOCATOR_CREATE_EXT_MEMORY_BUDGET_BIT
             | VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    ci.vulkanApiVersion = API_VERSION;
    ci.physicalDevice = physDevice;
    ci.device = device;
    ci.instance = instance;
    ci.pVulkanFunctions = &vf;

    VKCHECK(vmaCreateAllocator(&ci, &allocator));
}

void Renderer::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = gfxQueueFamily;

    VKCHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool));
}

void Renderer::waitForDevice() {
    vkDeviceWaitIdle(device);
}

void Renderer::updateSurfaceDimensions() {
    SDL_GetWindowSizeInPixels(window, &surfaceWidth, &surfaceHeight);
}

void Renderer::recordOneTime(std::function<void(VkCommandBuffer)> const& recordFunction) {
    VkCommandBufferAllocateInfo ai = {};
    ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    ai.commandPool = commandPool;
    ai.commandBufferCount = 1;

    VkCommandBuffer cmdbuf;
    vkAllocateCommandBuffers(device, &ai, &cmdbuf);

    VkCommandBufferBeginInfo bi = {};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdbuf, &bi);
    recordFunction(cmdbuf);
    vkEndCommandBuffer(cmdbuf);

    VkSubmitInfo si = {};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cmdbuf;

    vkQueueSubmit(gfxQueue, 1, &si, VK_NULL_HANDLE);
    vkQueueWaitIdle(gfxQueue);
    vkFreeCommandBuffers(device, commandPool, 1, &cmdbuf);
}