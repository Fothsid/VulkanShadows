/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Commonly used headers and macros.
///
#pragma once
#include <stdexcept>
#include <vulkan/vk_enum_string_helper.h>
#include <stdexcept>
#include <format>

#define ARRAY_COUNT(X) (sizeof(X)/sizeof((X)[0]))
#define VKCHECK(statement) do { \
        VkResult r = (statement);\
        if (r != VK_SUCCESS) {\
            throw std::runtime_error(std::format(#statement " returned {}.", string_VkResult(r)));\
        }\
    } while (0);
