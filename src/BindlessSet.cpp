/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Class for managing "bindless" VkImageView descriptors.
///
#include "Common.hpp"
#include "BindlessSet.hpp"

BindlessSet::BindlessSet(Renderer& renderer, uint32_t maxImageViews, uint32_t maxSamplers)
    : renderer(renderer)
    , maxImageViews(maxImageViews)
    , maxSamplers(maxSamplers)
    , bufferIndex(0)
    , imageViewCounter(0)
{
    uint32_t maxImageViewDescriptors = maxImageViews * IMAGEVIEW_BUFFER_COUNT;

    VkResult r;
    VkDescriptorPoolSize poolSizes[2] = {
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, maxImageViewDescriptors },
        { VK_DESCRIPTOR_TYPE_SAMPLER,       maxSamplers             }
    };

    //
    // Descriptor pool
    //

    VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    dpci.poolSizeCount = ARRAY_COUNT(poolSizes);
    dpci.pPoolSizes    = poolSizes;
    dpci.maxSets       = 1;

    r = vkCreateDescriptorPool(renderer.getDevice(), &dpci, nullptr, &pool);
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::format("Couldn't create a descriptor pool ({})", string_VkResult(r)));
        return;
    }

    //
    // Set layout
    //
    const uint32_t bindingCount = 2;
    VkDescriptorSetLayoutBinding bindings[bindingCount];
    memset(bindings, 0, sizeof(bindings));

    VkDescriptorBindingFlags bindingFlags[bindingCount] = {
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
        VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
    };

    // Sampler binding.
    bindings[0].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    bindings[0].descriptorCount = maxSamplers;
    bindings[0].binding         = SAMPLERS_BINDING;
    bindings[0].stageFlags      = VK_SHADER_STAGE_ALL;

    // Image binding.
    bindings[1].descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    bindings[1].descriptorCount = maxImageViewDescriptors;
    bindings[1].binding         = IMAGEVIEW_BINDING;
    bindings[1].stageFlags      = VK_SHADER_STAGE_ALL;

    VkDescriptorSetLayoutBindingFlagsCreateInfo flagInfo = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO
    };
    flagInfo.bindingCount  = bindingCount;
    flagInfo.pBindingFlags = bindingFlags;

    VkDescriptorSetLayoutCreateInfo layoutci = { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutci.pNext        = &flagInfo;
    layoutci.flags        = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutci.bindingCount = bindingCount;
    layoutci.pBindings    = bindings;
    VKCHECK(vkCreateDescriptorSetLayout(renderer.getDevice(), &layoutci, nullptr, &layout));

    //
    // Descriptor sets
    //
    VkDescriptorSetVariableDescriptorCountAllocateInfo vcountai = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO
    };
    vcountai.descriptorSetCount = 1;
    vcountai.pDescriptorCounts  = &maxImageViewDescriptors;

    VkDescriptorSetAllocateInfo setai = {
        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO
    };
    setai.pNext              = &vcountai;
    setai.pSetLayouts        = &layout;
    setai.descriptorPool     = pool;
    setai.descriptorSetCount = 1;

    r = vkAllocateDescriptorSets(renderer.getDevice(), &setai, &set);
    if (r != VK_SUCCESS) {
        throw std::runtime_error(std::format("Couldn't allocate bindless descriptor set ({})", string_VkResult(r)));
    }
}

BindlessSet::~BindlessSet() {
    vkDestroyDescriptorPool(renderer.getDevice(), pool, nullptr);
    vkDestroyDescriptorSetLayout(renderer.getDevice(), layout, nullptr);
    pool = VK_NULL_HANDLE;
    layout = VK_NULL_HANDLE;
}

BindlessSet::BindlessSet(BindlessSet&& orig)
    : renderer(orig.renderer)
    , pool(orig.pool)
    , layout(orig.layout)
    , set(orig.set)
    , maxImageViews(orig.maxImageViews)
    , maxSamplers(orig.maxSamplers)
{
    orig.pool   = VK_NULL_HANDLE;
    orig.layout = VK_NULL_HANDLE;
    orig.set    = VK_NULL_HANDLE;
}

void BindlessSet::clearImageViews() {
    imageViewCounter = 0;
    bufferIndex      = (bufferIndex + 1) % IMAGEVIEW_BUFFER_COUNT;
}

uint32_t BindlessSet::getImageViewCount() {
    return imageViewCounter;
}

uint32_t BindlessSet::getNextImageViewIndex() {
    return bufferIndex * maxImageViews + imageViewCounter;
}

uint32_t BindlessSet::addImageView(VkImageView imageView) {
    if (imageViewCounter >= maxImageViews) {
        throw std::runtime_error("Out of descriptors for image views in a bindless set.");
    }

    uint32_t index = getNextImageViewIndex();
    imageViewCounter++;

    setImageViewIndex(index, imageView);
    return index;
}

void BindlessSet::setImageViewIndex(uint32_t index, VkImageView imageView) {
    VkDescriptorImageInfo dii = {};
    dii.imageView   = imageView;
    dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet wds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds.dstSet          = set;
    wds.dstBinding      = IMAGEVIEW_BINDING;
    wds.dstArrayElement = index;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    wds.pImageInfo      = &dii;

    vkUpdateDescriptorSets(renderer.getDevice(), 1, &wds, 0, nullptr);
}

void BindlessSet::setSamplerIndex(uint32_t index, VkSampler sampler) {
    VkDescriptorImageInfo dii = {};
    dii.sampler     = sampler;
    dii.imageLayout = VK_IMAGE_LAYOUT_READ_ONLY_OPTIMAL;

    VkWriteDescriptorSet wds = { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
    wds.dstSet          = set;
    wds.dstBinding      = SAMPLERS_BINDING;
    wds.dstArrayElement = index;
    wds.descriptorCount = 1;
    wds.descriptorType  = VK_DESCRIPTOR_TYPE_SAMPLER;
    wds.pImageInfo      = &dii;
    
    vkUpdateDescriptorSets(renderer.getDevice(), 1, &wds, 0, nullptr);
}
