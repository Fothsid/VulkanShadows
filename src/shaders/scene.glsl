///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Commonly used data structure definitions for scene elements.
///
#ifndef SCENE_GLSL
#define SCENE_GLSL

#include "textures.glsl"

#extension GL_EXT_buffer_reference : require

struct Light {
    vec3  position;
    float intensity;
    vec3  ambient;
    float range;
    vec3  diffuse;
    uint  shadowMap;
    float zNear;
    float zFar;
};

layout(buffer_reference, std430) buffer Lights {
    Light data[];
};

layout(buffer_reference, std430) buffer Material {
    vec4  ambient;
    vec4  diffuse;
    float alphaCutoff;
    uint  samplerID;
    uint  baseColorTID;
};

layout(buffer_reference, std430) buffer Camera {
    vec3  eye;
    float fov;
    vec3  target;
    float aspectRatio;
    float depthNear;
    float depthFar;
    float padding0;
    float padding1;
    mat4  projection;
    mat4  view;
    mat4  projView;
};

layout (push_constant, std430) uniform PushConstants {
    mat4     transform;
    Camera   camera;
    Material material;
    Lights   lights;
    uint     lightCount;
    uint     textureBaseIndex;   // Scene texture index allocation start
    uint     currentLightID;     // Used only in shadowvolumes.geom
};

#endif