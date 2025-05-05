///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// VkShaderModule wrapper.
///
#include "Common.hpp"
#include <fstream>
#include <cstring>
#include "Shader.hpp"

Shader::Shader(Renderer& renderer, void* input, size_t dataSize)
    : device(renderer.getDevice())
    , module(create(device, input, dataSize))
{}

Shader::Shader(Renderer& renderer, std::istream& stream)
    : device(renderer.getDevice())
{
    stream.seekg(0, std::ios_base::end);
    
    std::vector<char> data(stream.tellg());
    stream.seekg(0);
    stream.read(data.data(), data.size());
    module = create(device, data.data(), data.size());
}

Shader::Shader(Renderer& renderer, const char* filename)
    : device(renderer.getDevice())
{
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error(std::format("Couldn't open the shader file '{}'", filename));
    }
    std::vector<char> data(file.tellg());
    file.seekg(0);
    file.read(data.data(), data.size());
    file.close();
    module = create(device, data.data(), data.size());
}

Shader::~Shader() {
    if (module && device) {
        vkDestroyShaderModule(device, module, nullptr);
    }
}

Shader::Shader(Shader&& orig)
    : device(orig.device)
    , module(orig.module)
{
    orig.device = VK_NULL_HANDLE;
    orig.module = VK_NULL_HANDLE;
}

VkShaderModule Shader::create(VkDevice device, void* data, size_t dataSize) {
    VkShaderModuleCreateInfo ci = { VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO };
    ci.codeSize = dataSize;
    ci.pCode = (uint32_t*) data;

    VkShaderModule m;
    VKCHECK(vkCreateShaderModule(device, &ci, nullptr, &m));
    return m;
}