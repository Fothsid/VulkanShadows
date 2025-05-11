/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Class for loading and drawing a glTF model.
///
#pragma once

#include "glm.hpp"
#include "Renderer.hpp"
#include "BindlessSet.hpp"
#include "GpuBuffer.hpp"
#include "Texture.hpp"
#include "ScenePipelines.hpp"
#include "VIBufferBuilder.hpp"
#include "tiny_gltf_wrap.hpp"
#include "Animation.hpp"
#include "Configuration.hpp"

enum eSceneDrawType {
    eSceneDrawType_Full,
    eSceneDrawType_Ambient,
    eSceneDrawType_DiffuseStencilTested,
    eSceneDrawType_ShadowMapped,
    eSceneDrawType_ShadowMap,
    eSceneDrawTypeCount
};

class Scene {
    static constexpr uint32_t MAX_LIGHTS = 32;
public:
    struct alignas(16) LightData {
        glm::vec3 position;
        float     intensity;
        glm::vec3 ambient;
        float     range;
        glm::vec3 diffuse;
        uint32_t  shadowMap;
        float     zNear;
        float     zFar;
    };

    struct alignas(16) MaterialData {
        glm::vec4 ambient;
        glm::vec4 diffuse;
        float     alphaCutoff;
        uint32_t  samplerID;
        uint32_t  baseColorTID;
    };

    struct alignas(16) CameraData {
        glm::vec3 eye;
        float     fov;
        glm::vec3 target;
        float     aspectRatio;
        float     depthNear;
        float     depthFar;
        float     padding0;
        float     padding1;
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 projView;
    };

    struct alignas(16) PushConstants {
        glm::mat4       transform;
        VkDeviceAddress camera;
        VkDeviceAddress material;
        VkDeviceAddress lights;
        uint32_t        lightCount;
        uint32_t        textureBaseIndex;
        uint32_t        currentLightID; // Used only when rendering shadow volumes
    };
    // Vulkan specification guarantees that the GPU will
    // support at least 128 bytes of push constants.
    // Limiting ourselves here to ensure maximum compatibility.
    static_assert(sizeof(PushConstants) <= 128);

    struct Node {
        void calculateLocalTransform();
        glm::vec3 translation;
        glm::quat rotation;
        glm::vec3 scale;
        glm::mat4 localTransform;
        glm::mat4 globalTransform;
    };
    
    struct Mesh {
        GpuVertexIndexBuffer      buffer;
        std::vector<VIBPrimGroup> primGroups;
    };

    struct ShadowMapConf {
        uint32_t resolution;
        bool     cullFrontFaces;
        float    biasConstant;
        float    biasSlope;
        float    zNear;
    };

    Scene(Renderer& renderer, const ScenePipelines& pipelines, const std::string& filename);

    /// Does not fill out samplers.
    void fillOutBindlessSet(BindlessSet& set);

    /// Records commands to draw a mesh into the command buffer.
    /// Used inside recordDrawCommands();
    void recordMeshDraw(VkCommandBuffer cmdbuf,
                        int nodeID,
                        uint32_t baseFlags = eScenePipelineFlags_Depth,
                        eSceneDrawType drawType = eSceneDrawType_Full);

    /// Lit scene draw functions. Descriptor set is assumed to be bound,
    /// swapchain render pass is assumed to be started.
    void recordScene(VkCommandBuffer cmdbuf,
                     uint32_t baseFlags = eScenePipelineFlags_Depth,
                     eSceneDrawType drawType = eSceneDrawType_Full);
    
    /// Draws the shadow volumes into the stencil buffer
    void recordShadowVolumesStencil(VkCommandBuffer cmdbuf, eSVMethod method, uint32_t lightID);

    /// Draws the edges of the calculated silhoutte.
    void recordSilhoutteDebugOverlay(VkCommandBuffer cmdbuf, uint32_t lightID);

    /// Draws the scene to light shadow maps. Should be called outside of a
    /// render pass. Descriptor set is assumed to be bound.
    void drawToShadowMaps(VkCommandBuffer cmdbuf, BindlessSet& set);

    /// Used by drawToShadowMaps()
    void recordCubeFace(VkCommandBuffer cmdbuf, int lightID, int faceID); 

    /// Records commands to upload camera/light data to the buffer.
    void recordDrawBufferUpdates(VkCommandBuffer cmdbuf);

    /// Used inside tinygltf image load callback.
    bool addDDSTexture(const unsigned char* bytes, uint32_t size);

    /// Used inside tinygltf image load callback.
    bool addSTBTexture(const unsigned char* bytes, uint32_t size);

    /// Advances all animations in the scene. Returns true if any of them end.
    bool advanceAnimations(float timestep, bool loop);

    /// Returns the final transform for a node.
    glm::mat4 getNodeTransform(int nodeID);

    /// Traverses the node tree and updates global transform matrices.
    void calculateGlobalTransforms();

    int lightNodeID;
    int cameraNodeID;
    ShadowMapConf shadowMapConf;
    CameraData camera;
    std::vector<LightData> lights;
    std::vector<TextureCubeShadowMap> shadowMaps;
private:
    void allocateBuffers();
    void loadMeshes();
    void loadMaterials();
    
    void propagateTransform(const glm::mat4& prev, int nodeID);
    void loadNodes();
    void loadAnimations();

    Renderer& renderer;
    
    const ScenePipelines& pipelines;
    
    tinygltf::TinyGLTF gltfLoader;
    tinygltf::Model    gltfModel;

    std::vector<Mesh>         meshes;
    std::vector<Texture2D>    textures;
    std::vector<Node>         nodes;
    std::vector<Animation>    animations;
    std::vector<MaterialData> materials;
    std::vector<int>          nodeDrawOrder;

    std::unique_ptr<GpuShaderBuffer> lightBuffer;
    std::unique_ptr<GpuShaderBuffer> cameraBuffer;
    std::unique_ptr<GpuShaderBuffer> materialBuffer;

    PushConstants pushConstants;
    VkPipeline    lastBoundPipeline;
};