/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// glTF animation player (excluding morph targets).
///
#pragma once

#include <unordered_map>
#include "glm.hpp"
#include "tiny_gltf_wrap.hpp"

struct AnimationTransform {
    AnimationTransform()
        : translation(0,0,0)
        , rotation(1,0,0,0)
        , scale(1,1,1)
        , translationAnimated(false)
        , rotationAnimated(false)
        , scaleAnimated(false)
    {}

    glm::mat4 getMatrix() {
        return glm::translate(glm::mat4(1), translation)
             * glm::toMat4(rotation)
             * glm::scale(glm::mat4(1), scale);
    }
    bool      translationAnimated;
    bool      rotationAnimated;
    bool      scaleAnimated;
    glm::vec3 translation;
    glm::quat rotation;
    glm::vec3 scale;
};

class Animation {
public:
    Animation(const tinygltf::Model& gltfModel, int animID);

    /// Advances time by `timestep` and resamples the transforms.
    bool advance(float timestep);

    /// Resets animation time.
    void reset();

    /// Maps node ID -> transform
    std::unordered_map<int, AnimationTransform> nodes;
protected:
    void resample();
    glm::vec4 readAccessor(const tinygltf::Accessor& accessor, size_t index, const glm::vec4& defaultValue = {0,0,0,0});
    glm::vec4 sampleSampler(const tinygltf::AnimationSampler& sampler, float sampleTime, const glm::vec4& defaultValue = {0,0,0,0}, bool useSlerp = false);
    
    const tinygltf::Model& gltfModel;
    
    int   animID;
    float time;
    float minTime, maxTime;
};