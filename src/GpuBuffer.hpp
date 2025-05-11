/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkBuffer abstractions for various use cases.
///
#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "Renderer.hpp"

/// Base class for VkBuffer and VmaAllocation
class GpuBuffer {
public:
    static constexpr VkBufferUsageFlags USAGE_VERTEXBUFFER = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                           | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    static constexpr VkBufferUsageFlags USAGE_INDEXBUFFER = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                          | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    static constexpr VkBufferUsageFlags USAGE_UNIFORMBUFFER = VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                            | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    static constexpr VkBufferUsageFlags USAGE_STAGING = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    static constexpr VkBufferUsageFlags USAGE_STORAGE = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT
                                                      | VK_BUFFER_USAGE_TRANSFER_DST_BIT
                                                      | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;

    static constexpr VmaAllocationCreateFlags STAGING_FLAGS = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                                            | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    static constexpr VmaAllocationCreateFlags GPUMEM_FLAGS = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

    GpuBuffer(Renderer& renderer,
              uint64_t size,
              VkBufferUsageFlags usage,
              VmaAllocationCreateFlags flags = GPUMEM_FLAGS,
              VmaMemoryUsage memUsage = VMA_MEMORY_USAGE_AUTO);
    ~GpuBuffer();
    GpuBuffer(const GpuBuffer&) = delete; // No copy constructor.
    GpuBuffer(GpuBuffer&& orig);          // Move constructor.

    void copyFrom(VkCommandBuffer cmdbuf, GpuBuffer& src, uint64_t size, uint64_t srcOffset = 0, uint64_t dstOffset = 0);
    VkBufferMemoryBarrier getBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t offset, uint32_t size) const;

    /// Implicit conversion to VkBuffer when passing to C Vulkan functions
    inline operator VkBuffer() const { return buffer; }

    uint64_t      getSize()       const { return size; }
    VkBuffer      getBuffer()     const { return buffer; }
    VmaAllocation getAllocation() const { return allocation; }
protected:
    Renderer&         renderer;
    uint64_t          size;
    VkBuffer          buffer;
    VmaAllocation     allocation;
    VmaAllocationInfo allocationInfo;
};

/// For passing data to shaders using buffer device address feature.
class GpuShaderBuffer : public GpuBuffer {
public:
    GpuShaderBuffer(Renderer& renderer, uint64_t size)
        : GpuBuffer(renderer, size, GpuBuffer::USAGE_STORAGE, GpuBuffer::GPUMEM_FLAGS)
    {
        VkBufferDeviceAddressInfo bdai = { VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO };
        bdai.buffer = buffer;
        gpuAddress = vkGetBufferDeviceAddress(renderer.getDevice(), &bdai);
    }

    VkDeviceAddress getGpuAddress() const { return gpuAddress; }
protected:
    VkDeviceAddress gpuAddress;
};

/// For storing vertex data.
class GpuVertexBuffer : public GpuBuffer {
public:
    GpuVertexBuffer(Renderer& renderer, uint64_t size)
        : GpuBuffer(renderer, size, GpuBuffer::USAGE_VERTEXBUFFER)
    {}

    inline void bind(VkCommandBuffer cmdbuf, uint32_t binding = 0, VkDeviceSize offset = 0) {
        vkCmdBindVertexBuffers(cmdbuf, binding, 1, &buffer, &offset);
    }
};

/// For storing index data.
class GpuIndexBuffer : public GpuBuffer {
public:
    GpuIndexBuffer(Renderer& renderer, uint64_t size)
        : GpuBuffer(renderer, size, GpuBuffer::USAGE_INDEXBUFFER)
    {}

    inline void bind(VkCommandBuffer cmdbuf, uint32_t offset = 0, VkIndexType indexType = VK_INDEX_TYPE_UINT32) {
        vkCmdBindIndexBuffer(cmdbuf, buffer, offset, indexType);
    }
};

/// For storing both vertex and index data.
class GpuVertexIndexBuffer : public GpuBuffer {
public:
    GpuVertexIndexBuffer(Renderer& renderer, uint64_t size)
        : GpuBuffer(renderer, size, GpuBuffer::USAGE_VERTEXBUFFER|GpuBuffer::USAGE_INDEXBUFFER)
    {}

    inline void bindVertexBuffer(VkCommandBuffer cmdbuf, uint32_t binding = 0, VkDeviceSize offset = 0) const {
        vkCmdBindVertexBuffers(cmdbuf, binding, 1, &buffer, &offset);
    }
    inline void bindIndexBuffer(VkCommandBuffer cmdbuf, uint32_t offset = 0, VkIndexType indexType = VK_INDEX_TYPE_UINT32) const {
        vkCmdBindIndexBuffer(cmdbuf, buffer, offset, indexType);
    }
};

/// For memory transfers between CPU and GPU.
class GpuStagingBuffer : public GpuBuffer {
public:
    GpuStagingBuffer(Renderer& renderer, uint64_t size)
        : GpuBuffer(renderer, size, GpuBuffer::USAGE_STAGING, GpuBuffer::STAGING_FLAGS)
    {}

    inline void* getMappedData() { return allocationInfo.pMappedData; }
};