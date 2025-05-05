///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Generation and storage of all VkPipeline objects used by the application.
///
#include <cstdio>
#include <iostream>
#include "Common.hpp"
#include "ScenePipelines.hpp"
#include "PipelineLayoutBuilder.hpp"
#include "PipelineBuilder.hpp"
#include "RenderPassBuilder.hpp"
#include "Shader.hpp"

static VkCullModeFlags cull_mode_from_scene_pipeline_flags(uint32_t flags) {
    VkCullModeFlags cullFlags = VK_CULL_MODE_NONE;
    if (flags & eScenePipelineFlags_CullBackFace) {
        cullFlags |= VK_CULL_MODE_BACK_BIT;
    }
    if (flags & eScenePipelineFlags_CullFrontFace) {
        cullFlags |= VK_CULL_MODE_FRONT_BIT;
    }
    return cullFlags;
}

ScenePipelines::ScenePipelines(Renderer& renderer, VkDescriptorSetLayout setLayout)
    : renderer(renderer)
{
    { // Main pipeline layout
        PipelineLayoutBuilder lb;
        lb.addPushConstantRange(VK_SHADER_STAGE_ALL_GRAPHICS, 0, 128); // Take all the guaranteed space we can get!
        lb.addDescriptorSetLayout(setLayout);
        layout = lb.create(renderer);
    }
    { // Shadow map render pass
        RenderPassBuilder rpb;
        rpb.addAttachment(renderer.getBestDepthFormat(),
                          VK_ATTACHMENT_LOAD_OP_CLEAR,
                          VK_ATTACHMENT_STORE_OP_STORE,
                          VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        rpb.addSubpass();
        rpb.setSubpassDepthStencilAttachment(0, 0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        constexpr VkSubpassDependency dependencyA = {
            .srcSubpass      = VK_SUBPASS_EXTERNAL,
            .dstSubpass      = 0,
            .srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
        };
        constexpr VkSubpassDependency dependencyB = {
            .srcSubpass      = 0,
            .dstSubpass      = VK_SUBPASS_EXTERNAL,
            .srcStageMask    = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask    = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            .srcAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask   = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        };
        rpb.addDependency(dependencyA);
        rpb.addDependency(dependencyB);
        shadowMapRenderPass = rpb.create(renderer);
    }

    PipelineBuilder plb;
    plb.addDynamicState(VK_DYNAMIC_STATE_VIEWPORT);
    plb.addDynamicState(VK_DYNAMIC_STATE_SCISSOR);
    plb.addVertexBinding(0, sizeof(VertexNT));
    plb.addVertexAttributesFromFlags(0, eVertexFlags_Normal | eVertexFlags_TexCoord);

    createMainScenePipelines(plb);
    createStencilShadowVolumePipelines(plb);
    createShadowMapPipelines(plb);

    valid = true;
}

void ScenePipelines::createMainScenePipelines(PipelineBuilder& plb) {
    plb.setRenderPass(renderer.getSwapchain().getRenderPass());
    plb.setLayout(layout);
    
    Shader sceneVS(renderer, "shaders/scene.vert.spirv");
    Shader sceneFS(renderer, "shaders/scene.frag.spirv");
    for (uint32_t i = 0; i <= eScenePipelineFlagsAll; ++i) {
        struct {
            uint32_t enableAlphaTest;
            uint32_t useShadowMaps;
            uint32_t outputAmbient;
            uint32_t outputDiffuse;
        } constants;

        static const VkSpecializationMapEntry mapEntries[] = {
            { 0, sizeof(uint32_t)*0, sizeof(uint32_t) }, // ENABLE_ALPHA_TEST
            { 1, sizeof(uint32_t)*1, sizeof(uint32_t) }, // USE_SHADOW_MAPS
            { 2, sizeof(uint32_t)*2, sizeof(uint32_t) }, // OUTPUT_AMBIENT
            { 3, sizeof(uint32_t)*3, sizeof(uint32_t) }, // OUTPUT_DIFFUSE
        };

        VkSpecializationInfo spec;
        spec.mapEntryCount = ARRAY_COUNT(mapEntries);
        spec.dataSize      = sizeof(constants);
        spec.pMapEntries   = mapEntries;
        spec.pData         = &constants;

        constants.enableAlphaTest = (i & eScenePipelineFlags_EnableAlphaTest);

        plb.clearShaderStages();
        plb.clearBlendAttachments();
        plb.addVertexShader(sceneVS, &spec);
        plb.addFragmentShader(sceneFS, &spec);
        plb.setDepthState((i & eScenePipelineFlags_EnableDepthTest) != 0,
                          (i & eScenePipelineFlags_EnableDepthWrite) != 0);
        plb.setCulling(cull_mode_from_scene_pipeline_flags(i), VK_FRONT_FACE_COUNTER_CLOCKWISE);
        plb.addBlendAttachment((i & eScenePipelineFlags_EnableBlend) != 0);
        plb.setStencilState(false);

        constants.useShadowMaps = false;
        constants.outputAmbient = true;
        constants.outputDiffuse = true;
        scene[i] = plb.create(renderer);

        constants.useShadowMaps = true;
        constants.outputAmbient = true;
        constants.outputDiffuse = true;
        sceneShadowMapped[i] = plb.create(renderer);

        // Ambient lighting scene pass.
        constants.useShadowMaps = false;
        constants.outputAmbient = true;
        constants.outputDiffuse = false;
        sceneAmbientOnly[i] = plb.create(renderer);

        static constexpr VkStencilOpState test = {
            .failOp      = VK_STENCIL_OP_KEEP,
            .passOp      = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp   = VK_COMPARE_OP_EQUAL,
            .compareMask = 0xff,
            .writeMask   = 0xff,
            .reference   = 0
        };

        // Stencil tested diffuse only with an additive blend mode.
        // To be used for adding shadows from rendered shadow volumes to the scene.
        constants.useShadowMaps = false;
        constants.outputAmbient = false;
        constants.outputDiffuse = true;
        plb.setDepthState(true, false, VK_COMPARE_OP_EQUAL);
        plb.setStencilState(true, test, test);
        plb.clearBlendAttachments();
        plb.addBlendAttachment(true, VK_BLEND_OP_ADD, VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE);
        sceneDiffuseOnlyST[i] = plb.create(renderer);
    }
}

void ScenePipelines::createShadowMapPipelines(PipelineBuilder& plb) {
    Shader smapVertexShader  (renderer, "shaders/shadowmap.vert.spirv");
    Shader smapFragmentShader(renderer, "shaders/shadowmap.frag.spirv");
    plb.setRenderPass(shadowMapRenderPass);
    plb.addDynamicState(VK_DYNAMIC_STATE_DEPTH_BIAS);
    plb.setDepthBias(true);

    // Ignore normals here since we're not using them in the shader.
    plb.clearVertexBindings();
    plb.addVertexBinding(0, sizeof(VertexNT));
    plb.addVertexAttributesFromFlags(0, eVertexFlags_Normal|eVertexFlags_TexCoord, eVertexFlags_Normal);

    for (uint32_t i = 0; i <= eScenePipelineFlagsAll; ++i) {
        static const VkSpecializationMapEntry mapEntries[] = {
            { 0, 0, sizeof(uint32_t) }, // ENABLE_ALPHA_TEST
        };
        uint32_t specData[ARRAY_COUNT(mapEntries)] = {
            (i & eScenePipelineFlags_EnableAlphaTest) != 0, // ENABLE_ALPHA_TEST
        };
        VkSpecializationInfo spec;
        spec.mapEntryCount = ARRAY_COUNT(mapEntries);
        spec.dataSize      = sizeof(specData);
        spec.pMapEntries   = mapEntries;
        spec.pData         = specData;

        plb.clearShaderStages();
        plb.addVertexShader(smapVertexShader, &spec);
        plb.addFragmentShader(smapFragmentShader, &spec);
        plb.setDepthState((i & eScenePipelineFlags_EnableDepthTest) != 0,
                          (i & eScenePipelineFlags_EnableDepthWrite) != 0);
        plb.setStencilState(false);
        plb.setCulling(cull_mode_from_scene_pipeline_flags(i), VK_FRONT_FACE_COUNTER_CLOCKWISE);
        shadowMap[i] = plb.create(renderer);
    }
}

void ScenePipelines::createStencilShadowVolumePipelines(PipelineBuilder& plb) {
    // Don't do any stencil testing, just increment/decrement stencil
    // depending on the depth test.
    constexpr VkStencilOpState depthPassFront = {
        .failOp      = VK_STENCIL_OP_KEEP,
        .passOp      = VK_STENCIL_OP_INCREMENT_AND_WRAP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp   = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0xff,
        .writeMask   = 0xff,
        .reference   = 0
    };
    constexpr VkStencilOpState depthPassBack = {
        .failOp      = VK_STENCIL_OP_KEEP,
        .passOp      = VK_STENCIL_OP_DECREMENT_AND_WRAP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp   = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0xff,
        .writeMask   = 0xff,
        .reference   = 0
    };
    constexpr VkStencilOpState depthFailFront = {
        .failOp      = VK_STENCIL_OP_KEEP,
        .passOp      = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_INCREMENT_AND_WRAP,
        .compareOp   = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0xff,
        .writeMask   = 0xff,
        .reference   = 0
    };
    constexpr VkStencilOpState depthFailBack = {
        .failOp      = VK_STENCIL_OP_KEEP,
        .passOp      = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_DECREMENT_AND_WRAP,
        .compareOp   = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0xff,
        .writeMask   = 0xff,
        .reference   = 0
    };

    Shader svolVertexShader        (renderer, "shaders/shadowvolumes.vert.spirv");
    Shader svolGeometryShader      (renderer, "shaders/shadowvolumes.geom.spirv");
    Shader silhoutteGeometryShader (renderer, "shaders/svsilhouette.geom.spirv");
    Shader silhoutteDebugShader    (renderer, "shaders/silhouettedebug.geom.spirv");
    Shader silhoutteDebugFragShader(renderer, "shaders/debug.frag.spirv");
    
    static const VkSpecializationMapEntry mapEntries[] = {
        { 0, sizeof(uint32_t)*0, sizeof(uint32_t) }, // FRONT_CAP
        { 1, sizeof(uint32_t)*1, sizeof(uint32_t) }, // BACK_CAP
        { 2, sizeof(uint32_t)*2, sizeof(uint32_t) }, // SIDES
    };

    struct {
        uint32_t frontCap;
        uint32_t backCap;
        uint32_t sides;
    } constants;

    VkSpecializationInfo spec;
    spec.mapEntryCount = ARRAY_COUNT(mapEntries);
    spec.dataSize      = sizeof(constants);
    spec.pMapEntries   = mapEntries;
    spec.pData         = &constants;

    plb.clearVertexBindings();
    plb.addVertexBinding(0, sizeof(VertexNT));
    plb.addVertexAttributesFromFlags(0, eVertexFlags_Normal|eVertexFlags_TexCoord, eVertexFlags_Normal|eVertexFlags_TexCoord);
    plb.setCulling(VK_CULL_MODE_NONE, VK_FRONT_FACE_COUNTER_CLOCKWISE);

    plb.clearShaderStages();
    plb.addVertexShader(svolVertexShader);
    plb.addGeometryShader(silhoutteDebugShader);
    plb.addFragmentShader(silhoutteDebugFragShader);
    plb.setRenderPass(renderer.getSwapchain().getRenderPass());
    plb.clearBlendAttachments();
    plb.addBlendAttachment(false);
    plb.setDepthState(true, false); // Don't care about blending, we'll be just testing against the depth buffer.
    plb.setStencilState(false);
    plb.setPrimitive(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY);
    silhoutteDebug = plb.create(renderer);

    // NOTE:
    // For correct rendering of the depth fail method we need to have
    // depth clamping enabled for the extruded part and the back cap,
    // but *disabled* for the front cap. Otherwise when geometry gets
    // close to the near plane artifacts start appearing due to the clamped
    // depth values not matching the depth values of the previously rendered
    // geometry in the depth buffer.
    //

    { // Silhoutte only
        plb.clearShaderStages();
        plb.addVertexShader(svolVertexShader);
        plb.addGeometryShader(silhoutteGeometryShader);
        plb.setPrimitive(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_WITH_ADJACENCY);

        plb.setStencilState(true, depthPassFront, depthPassBack);
        plb.setDepthClamp(false);
        svDPassSilhoutte = plb.create(renderer);

        plb.setStencilState(true, depthFailFront, depthFailBack);
        plb.setDepthClamp(true);
        svDFailSilhoutte = plb.create(renderer);
    }
    { // All triangles
        plb.clearShaderStages();
        plb.addVertexShader(svolVertexShader);
        plb.addGeometryShader(svolGeometryShader, &spec);
        plb.setPrimitive(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        // Draw volumes without caps for depth pass.
        constants.frontCap = false;
        constants.backCap  = false;
        constants.sides    = true;
        plb.setStencilState(true, depthPassFront, depthPassBack);
        plb.setDepthClamp(false);
        svDPass = plb.create(renderer);

        plb.setStencilState(true, depthFailFront, depthFailBack);
        plb.setDepthClamp(true);

        // Both back cap and sides of the volume
        constants.frontCap = false;
        constants.backCap  = true;
        constants.sides    = true;
        svDFailSidesBackCap = plb.create(renderer);
        
        // Only the outer cap.
        constants.frontCap = false;
        constants.backCap  = true;
        constants.sides    = false;
        svDFailBackCap = plb.create(renderer);

        // Only the front cap.
        plb.setDepthClamp(false);
        constants.frontCap = true;
        constants.backCap  = false;
        constants.sides    = false;
        svDFailFrontCap = plb.create(renderer);
    }
}

ScenePipelines::~ScenePipelines() {
    if (!valid)
        return;

    #define DESTROY(X) do { \
            vkDestroyPipeline(renderer.getDevice(), X, nullptr); \
            X = nullptr; \
        } while (0);
    #define DESTROY_ITERABLE(ITR) \
        for (auto& p : ITR) { \
            DESTROY(p); \
        }

    DESTROY_ITERABLE(scene);
    DESTROY_ITERABLE(sceneShadowMapped);
    DESTROY_ITERABLE(sceneAmbientOnly);
    DESTROY_ITERABLE(sceneDiffuseOnlyST);
    DESTROY_ITERABLE(shadowMap);

    DESTROY(silhoutteDebug);
    DESTROY(svDPass);
    DESTROY(svDPassSilhoutte);
    DESTROY(svDFailSilhoutte);
    DESTROY(svDFailFrontCap);
    DESTROY(svDFailSidesBackCap);
    DESTROY(svDFailBackCap);
    #undef DESTROY_ITERABLE
    #undef DESTROY

    vkDestroyPipelineLayout(renderer.getDevice(), layout, nullptr);
    vkDestroyRenderPass(renderer.getDevice(), shadowMapRenderPass, nullptr);
}

ScenePipelines::ScenePipelines(ScenePipelines&& o)
    : renderer(o.renderer)
{
    memcpy(this, &o, sizeof(*this));
    memset(&o, 0, sizeof(*this));
}