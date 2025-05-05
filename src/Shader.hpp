///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkShaderModule wrapper.
///
#pragma once

#include <istream>
#include "Renderer.hpp"

class Shader {
public:
    /// Creates a shader module from memory.
    Shader(Renderer& renderer, void* input, size_t dataSize);

    /// Creates a shader module from a specified input stream.
    Shader(Renderer& renderer, std::istream& stream);

    /// Creates a shader module from a specified file.
    Shader(Renderer& renderer, const char* filename);

    /// Destructor
    ~Shader();

    /// Copying not allowed
    Shader(const Shader&) = delete;

    /// Move constructor
    Shader(Shader&& orig);

    /// Implicit conversion to VkShaderModule when passing to C Vulkan functions
    inline operator VkShaderModule() const { return module; }

    /// Returns the stored shader module.
    inline VkShaderModule getModule() const { return module; }

private:
    static VkShaderModule create(VkDevice device, void* data, size_t dataSize);

    VkDevice device;
    VkShaderModule module;
};