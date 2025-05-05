/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Application configuration parsing and storage.
///
#pragma once

#include "glm.hpp"

enum eShadowTech : int {
    eShadowTech_None,
    eShadowTech_ShadowMapping,
    eShadowTech_StencilShadowVolumes,
    eShadowTechCount
};

enum eSVMethod {
    eSVMethod_DepthPass,
    eSVMethod_DepthFail,
    eSVMethod_SilhoutteDepthPass,
    eSVMethod_SilhoutteDepthFail,
    eSVMethodCount,
};

struct Configuration {
    Configuration(int argc, char** argv);
    static void printUsage(char* argv0);

    bool valid;

    const char* filename;

    int   width;
    int   height;
    int   gpuIndex;
    bool  resizable;
    bool  vsync;
    bool  help;
    bool  test;
    int   testFrames;
    float testTimeStep;

    bool      lightIgnoreNode;
    glm::vec3 lightPosition;
    glm::vec3 lightAmbient;
    glm::vec3 lightDiffuse;
    float     lightRange;
    float     lightIntensity;

    bool      cameraIgnoreNode;
    glm::vec3 cameraEye;
    glm::vec3 cameraTarget;
    float     cameraZNear;
    float     cameraZFar;
    float     cameraFov;

    eShadowTech shadowTech;
    eSVMethod   svMethod;
    bool        svDebugOverlay;
    int         smResolution;
    float       smBiasConstant;
    float       smBiasSlope;
    float       smZNear;
    bool        smPCFSampler;
    bool        smCullFrontFaces;
};