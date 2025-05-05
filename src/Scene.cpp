/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Class for loading and drawing a glTF model.
///
#include "Common.hpp"
#include "Scene.hpp"
#include "Vertex.hpp"
#include "CommonSamplers.hpp"
#include <stb_image.h>
#include <dds.hpp>
#include <stdexcept>
#include <iostream>
#include <format>
#include <vector>

static bool tinygltf_load_image_callback(tinygltf::Image* image, const int imageIdx, std::string* err,
                                  std::string* warn, int reqWidth, int reqHeight,
                                  const unsigned char* bytes, int size, void* userdata) {
    Scene* scene = reinterpret_cast<Scene*>(userdata);
    bool r = false;
    if (bytes[0] == 'D' && bytes[1] == 'D' && bytes[2] == 'S') {
        r = scene->addDDSTexture(bytes, uint32_t(size));
    } else {
        r = scene->addSTBTexture(bytes, uint32_t(size));
    }
    if (!r) {
        (*warn) += std::format("Error while loading texture %s. Replaced with a placeholder.\n", image->uri);
    }
    return r;
}

Scene::Scene(Renderer& renderer, const ScenePipelines& pipelines, const std::string& filename)
    : renderer(renderer)
    , pipelines(pipelines)
    , shadowMapConf({512, true, 512, 4, 0.1})
{
    if (filename.size() < 4) {
        throw std::runtime_error("glTF filename is too short.");
        return;
    }
    
    std::string err, warn;
    const char* check = filename.c_str()+filename.find_last_of('.');

    gltfLoader.SetImageLoader(tinygltf_load_image_callback, this);

    int ret;
    if (strcmp(check, ".gltf") == 0) {
        ret = gltfLoader.LoadASCIIFromFile(&gltfModel, &err, &warn, filename);
    } else if (strcmp(check, ".glb") == 0) {
        ret = gltfLoader.LoadBinaryFromFile(&gltfModel, &err, &warn, filename);
    } else {
        throw std::runtime_error(std::format("Could not determine the file format ({}).", check));
    }

    if (!warn.empty()) {
        std::cout << std::format("Warn: {}\n", warn);
    }

    if (!err.empty()) {
        std::cout << std::format("Err: {}\n", err);
    }

    if (!ret) {
        throw std::runtime_error("Could not load the glTF file.");
        return;
    }

    lightNodeID  = -1;
    cameraNodeID = -1;

    allocateBuffers();
    loadMeshes();
    loadMaterials();
    loadNodes();
    loadAnimations();
}

void Scene::fillOutBindlessSet(BindlessSet& set) {
    pushConstants.textureBaseIndex = set.getNextImageViewIndex();
    for (auto& t : textures) {
        set.addImageView(t.getView());
    }
}

void Scene::recordDrawBufferUpdates(VkCommandBuffer cmdbuf) {
    VkBufferMemoryBarrier before[] = {
        cameraBuffer->getBarrier(
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            0, sizeof(camera)
        ),
        lightBuffer->getBarrier(
            VK_ACCESS_SHADER_READ_BIT,
            VK_ACCESS_TRANSFER_WRITE_BIT,
            0, lights.size()*sizeof(LightData)
        ),
    };
    VkBufferMemoryBarrier after[] = {
        cameraBuffer->getBarrier(
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            0, sizeof(camera)
        ),
        lightBuffer->getBarrier( 
            VK_ACCESS_TRANSFER_WRITE_BIT,
            VK_ACCESS_SHADER_READ_BIT,
            0, lights.size()*sizeof(LightData)
        ),
    };
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, ARRAY_COUNT(before), before, 0, nullptr);
    vkCmdUpdateBuffer(cmdbuf, *cameraBuffer, 0, before[0].size, &camera);
    vkCmdUpdateBuffer(cmdbuf, *lightBuffer, 0, before[1].size, lights.data());
    vkCmdPipelineBarrier(cmdbuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_SHADER_BIT|VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT,
                         0, 0, nullptr, ARRAY_COUNT(after), after, 0, nullptr);
}

void Scene::recordMeshDraw(VkCommandBuffer cmdbuf, int meshID, uint32_t baseFlags, eSceneDrawType drawType) {
    const auto& mesh = meshes[meshID];
    for (const auto& group : mesh.primGroups) {
        pushConstants.material = materialBuffer->getGpuAddress() + sizeof(MaterialData) * group.materialID;

        uint32_t flags = baseFlags;

        if (group.materialID < gltfModel.materials.size()) {
            const auto& gltfMaterial = gltfModel.materials[group.materialID];
            if (!gltfMaterial.doubleSided) {
                if (drawType == eSceneDrawType_ShadowMap && shadowMapConf.cullFrontFaces) {
                    flags |= eScenePipelineFlags_CullFrontFace;
                } else {
                    flags |= eScenePipelineFlags_CullBackFace;
                }
            }
            switch (gltfMaterial.alphaMode[0]) {
            case 'M': // "MASK"
                flags |= eScenePipelineFlags_EnableAlphaTest;
                break;
            case 'B': // "BLEND"
                flags |= eScenePipelineFlags_EnableBlend;
                break;
            }
        }

        VkPipeline pipeline = VK_NULL_HANDLE;
        switch (drawType) {
        default:
        case eSceneDrawType_Full:
            pipeline = pipelines.scene[flags];
            break;
        case eSceneDrawType_ShadowMapped:
            pipeline = pipelines.sceneShadowMapped[flags];
            break;
        case eSceneDrawType_ShadowMap:
            pipeline = pipelines.shadowMap[flags];
            break;
        case eSceneDrawType_Ambient:
            pipeline = pipelines.sceneAmbientOnly[flags];
            break;
        case eSceneDrawType_DiffuseStencilTested:
            pipeline = pipelines.sceneDiffuseOnlyST[flags];
            break;
        }

        if (pipeline != lastBoundPipeline) {
            lastBoundPipeline = pipeline;
            vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        }

        mesh.buffer.bindVertexBuffer(cmdbuf, 0, group.vertexOffset);
        mesh.buffer.bindIndexBuffer(cmdbuf, group.indexOffset, VK_INDEX_TYPE_UINT32);
        vkCmdPushConstants(cmdbuf, pipelines.layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(PushConstants), &pushConstants);
        vkCmdDrawIndexed(cmdbuf, group.indexCount, 1, 0, 0, 0);
    }
}

void Scene::recordScene(VkCommandBuffer cmdbuf, uint32_t baseFlags, eSceneDrawType drawType) {
    lastBoundPipeline = VK_NULL_HANDLE;
    
    pushConstants.camera = cameraBuffer->getGpuAddress();
    pushConstants.lights = lightBuffer->getGpuAddress();
    pushConstants.lightCount = lights.size();

    for (auto i : nodeDrawOrder) {
        pushConstants.transform = nodes[i].globalTransform;
        recordMeshDraw(cmdbuf, gltfModel.nodes[i].mesh, baseFlags, drawType);
    }
}

void Scene::recordShadowVolumesStencil(VkCommandBuffer cmdbuf, eSVMethod method, uint32_t lightID) {
    lastBoundPipeline = VK_NULL_HANDLE;

    pushConstants.camera = cameraBuffer->getGpuAddress();
    pushConstants.lights = lightBuffer->getGpuAddress();
    pushConstants.lightCount = lights.size();
    pushConstants.currentLightID = lightID;
    
    VkPipeline passes  [3] = {VK_NULL_HANDLE};
    bool       useEdges[3] = {false};
    switch (method) {
    case eSVMethod_DepthPass:
        passes[0] = pipelines.svDPass;
        break;
    case eSVMethod_SilhoutteDepthPass:
        passes  [0] = pipelines.svDPassSilhoutte;
        useEdges[0] = true;
        break;
    case eSVMethod_DepthFail:
        passes[0] = pipelines.svDFailFrontCap;
        passes[1] = pipelines.svDFailSidesBackCap;
        break;
    case eSVMethod_SilhoutteDepthFail:
        passes  [0] = pipelines.svDFailFrontCap;
        passes  [1] = pipelines.svDFailSilhoutte;
        useEdges[1] = true;
        passes  [2] = pipelines.svDFailBackCap;
        break;
    }

    for (uint32_t passOrder = 0; passOrder < ARRAY_COUNT(passes); ++passOrder) {
        auto p = passes[passOrder];
        if (p == VK_NULL_HANDLE)
            break;
        vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, p);
        for (auto i : nodeDrawOrder) {
            pushConstants.transform = nodes[i].globalTransform;
            vkCmdPushConstants(cmdbuf, pipelines.layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pushConstants), &pushConstants);

            const auto& mesh = meshes[gltfModel.nodes[i].mesh];
            for (const auto& group : mesh.primGroups) {
                mesh.buffer.bindVertexBuffer(cmdbuf, 0, group.vertexOffset);
                if (useEdges[passOrder]) {
                    mesh.buffer.bindIndexBuffer(cmdbuf, group.edgeIndexOffset, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmdbuf, group.edgeIndexCount, 1, 0, 0, 0);
                } else {
                    mesh.buffer.bindIndexBuffer(cmdbuf, group.indexOffset, VK_INDEX_TYPE_UINT32);
                    vkCmdDrawIndexed(cmdbuf, group.indexCount, 1, 0, 0, 0);
                }
            }
        }
    }
}

void Scene::recordSilhoutteDebugOverlay(VkCommandBuffer cmdbuf, uint32_t lightID) {
    lastBoundPipeline = VK_NULL_HANDLE;
    pushConstants.camera         = cameraBuffer->getGpuAddress();
    pushConstants.lights         = lightBuffer->getGpuAddress();
    pushConstants.lightCount     = lights.size();
    pushConstants.currentLightID = lightID;
    vkCmdBindPipeline(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines.silhoutteDebug);
    for (auto i : nodeDrawOrder) {
        pushConstants.transform = nodes[i].globalTransform;
        vkCmdPushConstants(cmdbuf, pipelines.layout, VK_SHADER_STAGE_ALL_GRAPHICS, 0, sizeof(pushConstants), &pushConstants);

        const auto& mesh = meshes[gltfModel.nodes[i].mesh];
        for (const auto& group : mesh.primGroups) {
            mesh.buffer.bindVertexBuffer(cmdbuf, 0, group.vertexOffset);
            mesh.buffer.bindIndexBuffer(cmdbuf, group.edgeIndexOffset, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmdbuf, group.edgeIndexCount, 1, 0, 0, 0);
        }
    }
}

void Scene::drawToShadowMaps(VkCommandBuffer cmdbuf, BindlessSet& set) {
    lastBoundPipeline = VK_NULL_HANDLE;
    pushConstants.camera = cameraBuffer->getGpuAddress();
    if (shadowMaps.size() != lights.size()) {
        uint32_t howMany = lights.size() - shadowMaps.size();
        for (uint32_t i = 0; i < howMany; ++i) {
            shadowMaps.emplace_back(renderer, pipelines.shadowMapRenderPass, shadowMapConf.resolution);
        }
    }
    for (int l = 0; l < lights.size(); ++l) {
        LightData& light = lights[l];
        light.zNear = shadowMapConf.zNear;
        light.zFar  = light.range;
        for (int i = 0; i < 6; ++i) {
            recordCubeFace(cmdbuf, l, i);
        }
    }
    for (int l = 0; l < lights.size(); ++l) {
        lights[l].shadowMap = set.addImageView(shadowMaps[l].getView());
    }
}

void Scene::recordCubeFace(VkCommandBuffer cmdbuf, int lightID, int faceID) {
    const float pi = glm::pi<float>(); // 180 degrees
    const float halfpi = pi / 2.0f;    // 90 degrees

    LightData& light = lights[lightID];

    // View matrix calculation based on the Vulkan samples by
    // Sascha Willems at https://github.com/SaschaWillems/Vulkan
    CameraData lcam;
    lcam.projection = glm::perspective(halfpi, 1.0f, light.zNear, light.zFar);
    lcam.view = glm::mat4(1.0f);
    switch (faceID) {
    case 0: // +X
        lcam.view = glm::rotate(lcam.view, halfpi, {0, 1, 0});
        lcam.view = glm::rotate(lcam.view, pi, {1, 0, 0});
        break;
    case 1: // -X
        lcam.view = glm::rotate(lcam.view, -halfpi, {0, 1, 0});
        lcam.view = glm::rotate(lcam.view, pi, {1, 0, 0});
        break;
    case 2: // +Y
        lcam.view = glm::rotate(lcam.view, -halfpi, {1, 0, 0});
        break;
    case 3: // -Y
        lcam.view = glm::rotate(lcam.view, halfpi, {1, 0, 0});
        break;
    case 4: // +Z
        lcam.view = glm::rotate(lcam.view, pi, {1, 0, 0});
        break;
    case 5: // -Z
        lcam.view = glm::rotate(lcam.view, pi, {0, 0, 1});
        break;
    }
    lcam.view = glm::translate(lcam.view, -light.position);
    lcam.projView = lcam.projection * lcam.view;

    VkBufferMemoryBarrier bsbarrier = cameraBuffer->getBarrier(VK_ACCESS_SHADER_READ_BIT,
                                                               VK_ACCESS_TRANSFER_WRITE_BIT,
                                                               0, sizeof(lcam));
    VkBufferMemoryBarrier bdbarrier = cameraBuffer->getBarrier(VK_ACCESS_TRANSFER_WRITE_BIT,
                                                               VK_ACCESS_SHADER_READ_BIT,
                                                               0, sizeof(lcam));
    vkCmdPipelineBarrier(cmdbuf,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         0, 0, nullptr, 1, &bsbarrier, 0, nullptr);
    vkCmdUpdateBuffer(cmdbuf, *cameraBuffer, 0, sizeof(lcam), &lcam);
    vkCmdPipelineBarrier(cmdbuf,
                         VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
                         0, 0, nullptr, 1, &bdbarrier, 0, nullptr);
    
    VkClearValue clearValue;
    clearValue.depthStencil = { 1.0f, 0 };

    const auto& texture = shadowMaps[lightID];

    VkRenderPassBeginInfo brp = { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    brp.renderPass        = pipelines.shadowMapRenderPass;
    brp.framebuffer       = texture.getFramebuffer(faceID);
    brp.renderArea.offset = {0, 0};
    brp.renderArea.extent = {texture.getWidth(), texture.getHeight()};
    brp.clearValueCount   = 1;
    brp.pClearValues      = &clearValue;
    vkCmdBeginRenderPass(cmdbuf, &brp, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = texture.getWidth();
    viewport.height   = texture.getHeight();
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdbuf, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = brp.renderArea.extent;
    vkCmdSetScissor(cmdbuf, 0, 1, &scissor);
    vkCmdSetDepthBias(cmdbuf, shadowMapConf.biasConstant, 0.0f, shadowMapConf.biasSlope);

    lastBoundPipeline = VK_NULL_HANDLE;
    for (auto i : nodeDrawOrder) {
        pushConstants.transform = nodes[i].globalTransform;
        recordMeshDraw(cmdbuf, gltfModel.nodes[i].mesh, eScenePipelineFlags_Depth, eSceneDrawType_ShadowMap);
    }

    vkCmdEndRenderPass(cmdbuf);
}

void Scene::allocateBuffers() {
    materialBuffer = std::make_unique<GpuShaderBuffer>(
        renderer,
        sizeof(MaterialData) * gltfModel.materials.size()
    );
    lightBuffer = std::make_unique<GpuShaderBuffer>(
        renderer,
        sizeof(LightData) * MAX_LIGHTS
    );
    cameraBuffer = std::make_unique<GpuShaderBuffer>(
        renderer,
        sizeof(CameraData)
    );
}

void Scene::loadMeshes() {
    for (const auto& gltfMesh : gltfModel.meshes) {
        VIBufferBuilder builder(renderer, gltfModel, gltfMesh);
        auto& mesh = meshes.emplace_back(
            builder.create(),
            std::move(builder.groups)
        );
    }
}

static const unsigned char placeholder_texture[] = {
    // Magenta-black pattern
    0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF,
    0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0xFF,
};

bool Scene::addDDSTexture(const unsigned char* bytes, uint32_t size) {
    dds::Image ddsImage;
    if (dds::readImage((uint8_t*)bytes, size, &ddsImage) != 0) {
        // Reading failed, load in the placeholder texture instead.
        auto& texture = textures.emplace_back(renderer, VK_FORMAT_R8G8B8A8_UNORM, 2, 2);
        GpuStagingBuffer staging(renderer, sizeof(placeholder_texture));
        renderer.recordOneTime([&](VkCommandBuffer cmdbuf) {
            memcpy(staging.getMappedData(), placeholder_texture, sizeof(placeholder_texture));
            texture.copyFromBuffer(cmdbuf, staging.getBuffer(), 0, 0, 0, 0, 2, 2);
        });
        return false;
    }

    VkFormat format = dds::getVulkanFormat(ddsImage.format, true);
    uint32_t w = ddsImage.width;
    uint32_t h = ddsImage.height;

    auto& texture = textures.emplace_back(renderer, format, w, h);

    GpuStagingBuffer staging(renderer, ddsImage.mipmaps[0].size());
    renderer.recordOneTime([&](VkCommandBuffer cmdbuf) {
        memcpy(staging.getMappedData(), ddsImage.mipmaps[0].data(), ddsImage.mipmaps[0].size());
        texture.copyFromBuffer(cmdbuf, staging.getBuffer(), 0, 0, 0, 0, w, h);
    });
    return true;
}

bool Scene::addSTBTexture(const unsigned char* bytes, uint32_t size) {
    VkFormat format;
    int w, h, comp;
    bool noStb;

    const unsigned char* data = stbi_load_from_memory(bytes, size, &w, &h, &comp, 0);
    if (!data) {
        w     = 2;
        h     = 2;
        comp  = 4;
        noStb = true;
        data  = placeholder_texture;
    }

    uint32_t imageSize = 0;
    switch (comp) {
    default:
    case 1:
        format    = VK_FORMAT_R8_UNORM;
        imageSize = uint32_t(w * h);
        break;
    case 2:
        format    = VK_FORMAT_R8G8_UNORM;
        imageSize = uint32_t(w * h * 2);
        break;
    case 3:
        // NOTE: This is correct. See the conversion below.
        format    = VK_FORMAT_R8G8B8A8_UNORM;
        imageSize = uint32_t(w * h * 4);
        break;
    case 4:
        format    = VK_FORMAT_R8G8B8A8_UNORM;
        imageSize = uint32_t(w * h * 4);
        break;
    }

    auto& texture = textures.emplace_back(renderer, format, uint32_t(w), uint32_t(h));
    
    GpuStagingBuffer staging(renderer, imageSize);
    renderer.recordOneTime([&](VkCommandBuffer cmdbuf) {
        if (comp == 3) {
            // VK_FORMAT_R8G8B8_UNORM is not widely supported, so it's honestly
            // better to just convert it to VK_FORMAT_R8G8B8A8_UNORM.
            uint8_t* mapped = (uint8_t*) staging.getMappedData();
            for (int y = 0; y < h; ++y) {
                for (int x = 0; x < w; ++x) {
                    mapped[4 * (y * w + x) + 0] = data[3 * (y * w + x) + 0];
                    mapped[4 * (y * w + x) + 1] = data[3 * (y * w + x) + 1];
                    mapped[4 * (y * w + x) + 2] = data[3 * (y * w + x) + 2];
                    mapped[4 * (y * w + x) + 3] = 255;
                }
            }
        } else {
            memcpy(staging.getMappedData(), data, imageSize);
        }
        texture.copyFromBuffer(cmdbuf, staging.getBuffer(), 0, 0, 0, 0, uint32_t(w), uint32_t(h));
    });

    if (!noStb) {
        stbi_image_free((void*)data);
        return true;
    } else {
        return false;
    }
}

bool Scene::advanceAnimations(float timestep, bool loop) {
    bool finished = false;
    for (auto& anim : animations) {
        if (anim.advance(timestep)) {
            finished = true;
            if (loop) {
                anim.reset();
            }
        }
        for (auto& pair : anim.nodes) {
            auto& nd = nodes[pair.first];
            auto& transform = pair.second;
            if (transform.translationAnimated) {
                nd.translation = transform.translation;
            }
            if (transform.rotationAnimated) {
                nd.rotation = transform.rotation;
            }
            if (transform.scaleAnimated) {
                nd.scale = transform.scale;
            }
        }
    }
    calculateGlobalTransforms();
    return finished;
}

glm::mat4 Scene::getNodeTransform(int nodeID) {
    if (nodeID < 0 || nodeID >= nodes.size())
        return glm::mat4(1.0f);
    return nodes[nodeID].globalTransform;
}

void Scene::loadMaterials() {
    for (const auto& gltfMaterial : gltfModel.materials) {
        auto& material = materials.emplace_back();
        const auto& pbr = gltfMaterial.pbrMetallicRoughness;
        glm::vec4 baseColorFactor = {
            float(pbr.baseColorFactor[0]),
            float(pbr.baseColorFactor[1]),
            float(pbr.baseColorFactor[2]),
            float(pbr.baseColorFactor[3]),
        };
        glm::vec4 emissiveFactor = {
            float(gltfMaterial.emissiveFactor[0]),
            float(gltfMaterial.emissiveFactor[1]),
            float(gltfMaterial.emissiveFactor[2]),
            1.0f
        };

        material.baseColorTID = 0;
        if (pbr.baseColorTexture.index >= 0) {
            material.baseColorTID = gltfModel.textures[pbr.baseColorTexture.index].source + 1;
        }
        material.alphaCutoff  = gltfMaterial.alphaMode[0] == 'M' ? gltfMaterial.alphaCutoff : 0;
        material.samplerID    = eSampler_Linear;
        
        // Trying to adapt the pbrMetallicRoughness model to the classic Phong model.
        material.ambient  = emissiveFactor;
        material.diffuse  = baseColorFactor;
        material.specular = glm::mix(glm::vec4(1), baseColorFactor, float(pbr.metallicFactor));
    }

    GpuStagingBuffer staging(renderer, materialBuffer->getSize());
    memcpy(staging.getMappedData(), materials.data(), staging.getSize());
    renderer.recordOneTime([&](VkCommandBuffer cmdbuf){
        materialBuffer->copyFrom(cmdbuf, staging, staging.getSize(), 0);
    });
}

void Scene::calculateGlobalTransforms() {
    for (const auto& gltfScene : gltfModel.scenes) {
        for (const auto& rootNodeID : gltfScene.nodes) {
            propagateTransform(glm::mat4(1.0), rootNodeID);
        }
    }
}

void Scene::propagateTransform(const glm::mat4& prev, int nodeID) {
    const auto& node = gltfModel.nodes[nodeID];
    auto& nd = nodes[nodeID];

    nd.calculateLocalTransform();
    nd.globalTransform = prev * nd.localTransform;
    for (int child : node.children) {
        propagateTransform(nd.globalTransform, child);
    }
}

void Scene::loadNodes() {
    nodeDrawOrder.clear();
    nodes.resize(gltfModel.nodes.size());
    for (int nodeID = 0; nodeID < nodes.size(); ++nodeID) {
        const auto& node = gltfModel.nodes[nodeID];
        auto& nd = nodes[nodeID];
        
        if (node.mesh >= 0) {
            nodeDrawOrder.push_back(nodeID);
        }

        if (node.camera >= 0) {
            cameraNodeID = nodeID;
        }

        if (node.light >= 0) {
            lightNodeID = nodeID;
        }

        nd.translation = {0,0,0};
        nd.rotation    = {1,0,0,0};
        nd.scale       = {1,1,1};
        if (node.matrix.size() == 16) {
            auto doubleData = node.matrix.data();
            auto floatData = &nd.localTransform[0][0];
            for (int i = 0; i < 16; ++i) {
                floatData[i] = float(doubleData[i]);
            }
        } else {
            if (node.translation.size() == 3) {
                nd.translation = {
                    float(node.translation[0]),
                    float(node.translation[1]),
                    float(node.translation[2])
                };
            }
            if (node.rotation.size() == 4) {
                nd.rotation = {
                    float(node.rotation[3]),
                    float(node.rotation[0]),
                    float(node.rotation[1]),
                    float(node.rotation[2])
                };
            }
            if (node.scale.size() == 3) {
                nd.scale = {
                    float(node.scale[0]),
                    float(node.scale[1]),
                    float(node.scale[2])
                };   
            }
        }
    }
    calculateGlobalTransforms();
}

void Scene::loadAnimations() {
    for (int i = 0; i < gltfModel.animations.size(); i++) {
        animations.emplace_back(gltfModel, i);
    }
}

void Scene::Node::calculateLocalTransform() {
    localTransform = glm::translate(glm::mat4(1), translation)
                   * glm::toMat4(rotation)
                   * glm::scale(glm::mat4(1), scale);
}
