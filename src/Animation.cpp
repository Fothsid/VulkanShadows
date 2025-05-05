/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// glTF animation player (excluding morph targets).
///
#include "Animation.hpp"
#include <cstdint>

Animation::Animation(const tinygltf::Model& gltfModel, int animID)
    : gltfModel(gltfModel)
    , animID(animID)
    , time(0)
    , minTime(-1)
    , maxTime(-1)
{
    // Fetch animation time range.
    const auto& animation = gltfModel.animations[animID];
    for (const auto& sampler : animation.samplers) {
        if (sampler.input < 0 || sampler.input >= gltfModel.accessors.size())
            continue;

        const auto& accessor = gltfModel.accessors[sampler.input];
        if (accessor.minValues.size() != 1 || accessor.maxValues.size() != 1)
            continue;

        if (minTime < 0 || minTime > accessor.minValues[0]) {
            minTime = accessor.minValues[0];
        }
        if (maxTime < 0 || maxTime < accessor.maxValues[0]) {
            maxTime = accessor.maxValues[0];
        }
    }

    // Sample animation at the starting time.
    time = minTime;
    resample();
}

bool Animation::advance(float timestep) {
    if (time >= maxTime)
        return true;
    time += timestep;
    resample();
    return false;
}

void Animation::reset() {
    time = minTime;
    resample();
}

void Animation::resample() {
    const auto& animation = gltfModel.animations[animID];
    for (const auto& channel : animation.channels) {
        const auto& sampler = animation.samplers[channel.sampler];
        if (channel.target_node < 0 || channel.target_node >= gltfModel.nodes.size())
            continue;
        AnimationTransform& transform = nodes[channel.target_node];
        
        glm::vec4 sample;
        switch (channel.target_path[0]) {
        default:
            continue;
        case 't':
            sample = sampleSampler(sampler, time, {0,0,0,1});
            transform.translation = glm::vec3(sample);
            transform.translationAnimated = true;
            break;
        case 'r':
            sample = sampleSampler(sampler, time, {1,0,0,0}, true);
            transform.rotation = glm::quat(sample.w, sample.x, sample.y, sample.z);
            transform.rotationAnimated = true;
            break;
        case 's':
            sample = sampleSampler(sampler, time, {1,1,1,1});
            transform.scale = glm::vec3(sample);
            transform.scaleAnimated = true;
            break;
        } 
    }
}

glm::vec4 Animation::readAccessor(const tinygltf::Accessor& accessor, size_t index, const glm::vec4& defaultValue) {
    if (index >= accessor.count)
        return defaultValue;

    int componentCount;
    switch (accessor.type) {
    case TINYGLTF_TYPE_SCALAR: componentCount = 1; break;
    case TINYGLTF_TYPE_VEC2:   componentCount = 2; break;
    case TINYGLTF_TYPE_VEC3:   componentCount = 3; break;
    case TINYGLTF_TYPE_VEC4:   componentCount = 4; break;
    default:
        return defaultValue;
    }

    const auto& view   = gltfModel.bufferViews[accessor.bufferView];
    const auto& buffer = gltfModel.buffers[view.buffer];
    const auto  data   = buffer.data.data() + view.byteOffset + accessor.byteOffset;

    // Assume non-floating point component types are normalized and convert
    // them according to specification.
    // glTF specification says that only the normalized variants are supported
    // for animation data (for rotation and weight paths only).

    glm::vec4 v = defaultValue;
    switch (accessor.componentType) {
    default:
        return defaultValue;
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        for (int i = 0; i < componentCount; ++i) {
            v[i] = reinterpret_cast<const float*>(data)[index * componentCount + i];
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_SHORT:
        for (int i = 0; i < componentCount; ++i) {
            v[i] = reinterpret_cast<const int16_t*>(data)[index * componentCount + i];
            v[i] = glm::max(v[i] / 32767.0f, -1.0f);
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        for (int i = 0; i < componentCount; ++i) {
            v[i] = reinterpret_cast<const uint16_t*>(data)[index * componentCount + i];
            v[i] = v[i] / 65535.0f;
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_BYTE:
        for (int i = 0; i < componentCount; ++i) {
            v[i] = reinterpret_cast<const int8_t*>(data)[index * componentCount + i];
            v[i] = glm::max(v[i] / 127.0f, -1.0f);
        }
        break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        for (int i = 0; i < componentCount; ++i) {
            v[i] = reinterpret_cast<const uint8_t*>(data)[index * componentCount + i];
            v[i] = v[i] / 255.0f;
        }
        break;
    }
    return v;
}

glm::vec4 Animation::sampleSampler(const tinygltf::AnimationSampler& sampler, float sampleTime, const glm::vec4& defaultValue, bool useSlerp) {
    if (sampler.input < 0 || sampler.input >= gltfModel.accessors.size())
        return defaultValue;
    if (sampler.output < 0 || sampler.output >= gltfModel.accessors.size())
        return defaultValue;
    
    const auto& timeAccessor  = gltfModel.accessors[sampler.input];
    const auto& valueAccessor = gltfModel.accessors[sampler.output];
    if (timeAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT ||
        timeAccessor.type != TINYGLTF_TYPE_SCALAR)
        return defaultValue;

    // NOTE:
    // Cubic spline keyframes have additional tangent values, hence the 
    // keyframeValueSize and keyframeValueOffset variables.
    size_t keyframeValueOffset = sampler.interpolation[0] == 'C' ? 1 : 0;
    size_t keyframeValueSize   = sampler.interpolation[0] == 'C' ? 3 : 1;
    size_t keyframeCount       = timeAccessor.count;

    float firstKeyframeTime = readAccessor(timeAccessor, 0).x;
    float lastKeyframeTime  = readAccessor(timeAccessor, keyframeCount-1).x;
    if (sampleTime < firstKeyframeTime) {
        return readAccessor(valueAccessor, keyframeValueOffset, defaultValue);
    } else if (sampleTime > lastKeyframeTime) {
        return readAccessor(valueAccessor, (keyframeCount-1)*keyframeValueSize + keyframeValueOffset, defaultValue);
    }

    // Find the keyframe pair.
    size_t keyframeIndex = 0;
    float  tnext;
    while (keyframeIndex + 1 < keyframeCount) {
        tnext = readAccessor(timeAccessor, keyframeIndex+1).x;
        if (sampleTime < tnext) {
            break;
        } else {
            keyframeIndex++;
        }
    }
    
    float     tprev = readAccessor(timeAccessor, keyframeIndex).x;
    float     t     = (sampleTime - tprev) / (tnext - tprev);
    float     t2, t3;
    glm::vec4 vprev, vnext; // keyframe values
    glm::vec4 cprev, cnext; // keyframe tangents

    switch (sampler.interpolation[0]) {
    default:
        return defaultValue;
    case 'S': // STEP
        return readAccessor(valueAccessor, keyframeIndex, defaultValue);
    case 'L': // LINEAR
        vprev = readAccessor(valueAccessor, keyframeIndex, defaultValue);
        vnext = readAccessor(valueAccessor, keyframeIndex+1, defaultValue);
        if (useSlerp) {
            glm::quat qprev   = glm::quat(vprev.w, vprev.x, vprev.y, vprev.z);
            glm::quat qnext   = glm::quat(vnext.w, vnext.x, vnext.y, vnext.z);
            glm::quat slerped = glm::slerp(qprev, qnext, t);
            return glm::vec4(slerped.x, slerped.y, slerped.z, slerped.w);
        } else {
            return glm::mix(vprev, vnext, t);
        }
    case 'C': // CUBICSPLINE
        vprev = readAccessor(valueAccessor, keyframeIndex*3 + 1, defaultValue);
        cprev = readAccessor(valueAccessor, keyframeIndex*3 + 2, defaultValue);
        vnext = readAccessor(valueAccessor, keyframeIndex*3 + 3, defaultValue);
        cnext = readAccessor(valueAccessor, keyframeIndex*3 + 4, defaultValue);
        t2 = t * t;
        t3 = t2 * t;
        return (2 * t3 - 3 * t2 + 1) * vprev + (t3 - 2 * t2 + t) * cprev + (-2 * t3 + 3 * t2) * vnext + (t3 - t2) * cnext;
    }
}