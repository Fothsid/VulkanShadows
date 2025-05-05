/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkBuffer abstractions for various use cases.
///
#include "Common.hpp"
#include "GpuBuffer.hpp"

GpuBuffer::GpuBuffer(Renderer& renderer,
                     uint64_t size,
                     VkBufferUsageFlags usage,
                     VmaAllocationCreateFlags flags,
                     VmaMemoryUsage memUsage)
    : renderer(renderer)
    , size(size)
{
    VkBufferCreateInfo bci = {};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size  = size;
    bci.usage = usage;
     
    VmaAllocationCreateInfo aci = {};
    aci.usage = memUsage;
    aci.flags = flags;

    VKCHECK(vmaCreateBuffer(renderer.getAllocator(), &bci, &aci, &buffer, &allocation, &allocationInfo));
}

GpuBuffer::~GpuBuffer() {
    if (allocation && buffer) {
        vmaDestroyBuffer(renderer.getAllocator(), buffer, allocation);
        buffer = VK_NULL_HANDLE;
        allocation = VK_NULL_HANDLE;
    }
}

GpuBuffer::GpuBuffer(GpuBuffer&& orig)
    : renderer(orig.renderer)
    , size(orig.size)
    , buffer(orig.buffer)
    , allocation(orig.allocation)
    , allocationInfo(orig.allocationInfo)
{
    orig.buffer = VK_NULL_HANDLE;
    orig.allocation = VK_NULL_HANDLE;
}

void GpuBuffer::copyFrom(VkCommandBuffer cmdbuf, GpuBuffer& src, uint64_t size, uint64_t srcOffset, uint64_t dstOffset) {
    VkBufferCopy region = {};
    region.srcOffset = srcOffset;
    region.dstOffset = dstOffset;
    region.size      = size;
    vkCmdCopyBuffer(cmdbuf, src.buffer, buffer, 1, &region);
}

VkBufferMemoryBarrier GpuBuffer::getBarrier(VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t offset, uint32_t size) const {
    return {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = srcAccessMask,
        .dstAccessMask = dstAccessMask,
        .srcQueueFamilyIndex = renderer.getQueueFamily(),
        .dstQueueFamilyIndex = renderer.getQueueFamily(),
        .buffer = buffer,
        .offset = offset,
        .size = size
    };
}