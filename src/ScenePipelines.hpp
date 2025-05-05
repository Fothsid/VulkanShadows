/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Generation and storage of all VkPipeline objects used by the application.
///
#pragma once

#include "Renderer.hpp"

enum eScenePipelineFlags {
    eScenePipelineFlags_CullBackFace     = (1 << 0),
    eScenePipelineFlags_CullFrontFace    = (1 << 1),
    eScenePipelineFlags_EnableAlphaTest  = (1 << 2),
    eScenePipelineFlags_EnableDepthTest  = (1 << 3),
    eScenePipelineFlags_EnableDepthWrite = (1 << 4), 
    eScenePipelineFlags_EnableBlend      = (1 << 5),

    // Every flag enabled.
    eScenePipelineFlagsAll = (1 << 6)-1,

    // Alias
    eScenePipelineFlags_Depth = eScenePipelineFlags_EnableDepthTest
                              | eScenePipelineFlags_EnableDepthWrite
};

struct PipelineBuilder;
struct ScenePipelines {
    /// Generates VkPipeline objects along with other objects
    /// for all possible configurations of eScenePipelineShader
    /// and eScenePipelineFlags.
    ScenePipelines(Renderer& renderer, VkDescriptorSetLayout setLayout);
    
    /// Destructor
    ~ScenePipelines();

    /// Copying not allowed.
    ScenePipelines(const ScenePipelines&) = delete;
    
    /// Move constructor
    ScenePipelines(ScenePipelines&&);

private:
    void createMainScenePipelines(PipelineBuilder& plb);
    void createShadowMapPipelines(PipelineBuilder& plb);
    void createStencilShadowVolumePipelines(PipelineBuilder& plb);
public:
    bool valid;
    VkPipelineLayout layout;          /// Common pipeline layout.
    VkRenderPass shadowMapRenderPass; /// Render pass to use when drawing to shadow maps.

    VkPipeline scene[eScenePipelineFlagsAll+1];              // Shadowless scene
    VkPipeline sceneShadowMapped[eScenePipelineFlagsAll+1];  // Scene with shadow maps applied.
    VkPipeline sceneAmbientOnly[eScenePipelineFlagsAll+1];   // Scene with only ambient lighting.
    VkPipeline sceneDiffuseOnlyST[eScenePipelineFlagsAll+1]; // Scene with only stencil-tested diffuse lighting.
    VkPipeline shadowMap[eScenePipelineFlagsAll+1];          // For drawing to shadow maps.

    VkPipeline silhoutteDebug;      // Silhoutte debugging lines
    VkPipeline svDPass;             // Depth Pass
    VkPipeline svDPassSilhoutte;    // Depth Pass but for silhoutte only
    VkPipeline svDFailSilhoutte;    // Depth Fail but for silhoutte only
    VkPipeline svDFailFrontCap;     // Front Cap (depth clamp disabled)
    VkPipeline svDFailSidesBackCap; // Volume + Back Cap (depth clamp enabled)
    VkPipeline svDFailBackCap;      // Back Cap (depth clamp enabled)

    Renderer& renderer;
};