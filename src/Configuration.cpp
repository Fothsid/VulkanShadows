/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Application configuration parsing and storage.
///
#include "Configuration.hpp"
#include <iostream>
#include <cstring>
#include <format>

static const char* fetch_option_arg(int& i, int argc, char** argv) {
    i++;
    if (i < argc) {
        return argv[i];
    } else {
        return nullptr;
    }
}

Configuration::Configuration(int argc, char** argv)
    : valid(false)
    , width(1280)
    , height(720)
    , gpuIndex(-1)
    , resizable(true)
    , vsync(false)
    , help(false)
    , test(false)
    , testFrames(300)
    , testTimeStep(1.0f/60.0f)
    , lightIgnoreNode(false)
    , lightPosition(0,0,0)
    , lightAmbient(0.3f, 0.3f, 0.5f)
    , lightDiffuse(0.7f, 0.5f, 0.5f)
    , lightRange(100.0f)
    , lightIntensity(2.0f)
    , cameraIgnoreNode(false)
    , cameraEye(0.0f,1.0f,0.0f)
    , cameraTarget(0.0f,1.0f,1.0f)
    , cameraZNear(0.25f)
    , cameraZFar(1000.0f)
    , cameraFov(45.0f)
    , shadowTech(eShadowTech_None)
    , svMethod(eSVMethod_SilhoutteDepthFail)
    , svDebugOverlay(false)
    , smResolution(512)
    , smBiasConstant(512)
    , smBiasSlope(4)
    , smZNear(0.1f)
    , smPCFSampler(true)
    , smCullFrontFaces(true)
{
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (arg[0] == '-' && arg[1] == '-') {
            const char* option    = arg + 2;
            const char* optionArg = nullptr;
            if (strcmp(option, "no-resizable") == 0) {
                resizable = false;
            } else if (strcmp(option, "resizable") == 0) {
                resizable = true;
            } else if (strcmp(option, "no-vsync") == 0) {
                vsync = false;
            } else if (strcmp(option, "vsync") == 0) {
                vsync = true;
            } else if (strcmp(option, "no-test") == 0) {
                test = false;
            } else if (strcmp(option, "test") == 0) {
                test = true;
            } else if (strcmp(option, "help") == 0) {
                help = true;
            } else if (strcmp(option, "camera-ignore-node") == 0) {
                cameraIgnoreNode = true;
            } else if (strcmp(option, "light-ignore-node") == 0) {
                lightIgnoreNode = true;
            } else if (strcmp(option, "no-sv-debug-overlay") == 0) {
                svDebugOverlay = false;
            } else if (strcmp(option, "sv-debug-overlay") == 0) {
                svDebugOverlay = true;
            } else if (strcmp(option, "no-sm-pcf") == 0) {
                smPCFSampler = false;
            } else if (strcmp(option, "sm-pcf") == 0) {
                smPCFSampler = true;
            } else if (strcmp(option, "no-sm-cull-front") == 0) {
                smCullFrontFaces = false;
            } else if (strcmp(option, "sm-cull-front") == 0) {
                smCullFrontFaces = true;
            } else if (strcmp(option, "width") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    width = atoi(optionArg);
                }
            } else if (strcmp(option, "height") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    height = atoi(optionArg);
                }
            } else if (strcmp(option, "gpu-index") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    gpuIndex = atoi(optionArg);
                }
            } else if (strcmp(option, "light-position") == 0) {
                const char* xstr = fetch_option_arg(i, argc, argv);
                const char* ystr = fetch_option_arg(i, argc, argv);
                const char* zstr = fetch_option_arg(i, argc, argv);
                if (xstr && ystr && zstr) {
                    lightPosition.x = atof(xstr);
                    lightPosition.y = atof(ystr);
                    lightPosition.z = atof(zstr);
                }
            } else if (strcmp(option, "light-ambient") == 0) {
                const char* rstr = fetch_option_arg(i, argc, argv);
                const char* gstr = fetch_option_arg(i, argc, argv);
                const char* bstr = fetch_option_arg(i, argc, argv);
                if (rstr && gstr && bstr) {
                    lightAmbient.r = atof(rstr);
                    lightAmbient.g = atof(gstr);
                    lightAmbient.b = atof(bstr);
                }
            } else if (strcmp(option, "light-diffuse") == 0) {
                const char* rstr = fetch_option_arg(i, argc, argv);
                const char* gstr = fetch_option_arg(i, argc, argv);
                const char* bstr = fetch_option_arg(i, argc, argv);
                if (rstr && gstr && bstr) {
                    lightDiffuse.r = atof(rstr);
                    lightDiffuse.g = atof(gstr);
                    lightDiffuse.b = atof(bstr);
                }
            } else if (strcmp(option, "light-range") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    lightRange = atof(optionArg);
                }
            } else if (strcmp(option, "light-intensity") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    lightIntensity = atof(optionArg);
                }
            } else if (strcmp(option, "camera-eye") == 0) {
                const char* xstr = fetch_option_arg(i, argc, argv);
                const char* ystr = fetch_option_arg(i, argc, argv);
                const char* zstr = fetch_option_arg(i, argc, argv);
                if (xstr && ystr && zstr) {
                    cameraEye.x = atof(xstr);
                    cameraEye.y = atof(ystr);
                    cameraEye.z = atof(zstr);
                }
            } else if (strcmp(option, "camera-target") == 0) {
                const char* xstr = fetch_option_arg(i, argc, argv);
                const char* ystr = fetch_option_arg(i, argc, argv);
                const char* zstr = fetch_option_arg(i, argc, argv);
                if (xstr && ystr && zstr) {
                    cameraTarget.x = atof(xstr);
                    cameraTarget.y = atof(ystr);
                    cameraTarget.z = atof(zstr);
                }
            } else if (strcmp(option, "camera-z-near") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    cameraZNear = atof(optionArg);
                }
            } else if (strcmp(option, "camera-z-far") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    cameraZFar = atof(optionArg);
                }
            } else if (strcmp(option, "camera-fov") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    cameraFov = atof(optionArg);
                }
            } else if (strcmp(option, "test-frames") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    testFrames = atoi(optionArg);
                }
            } else if (strcmp(option, "test-timestep") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    testTimeStep = atof(optionArg);
                }
            } else if (strcmp(option, "shadow-tech") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    if (strcmp(optionArg, "svdp") == 0) {
                        shadowTech = eShadowTech_StencilShadowVolumes;
                        svMethod   = eSVMethod_DepthPass;
                    } else if (strcmp(optionArg, "svdf") == 0) {
                        shadowTech = eShadowTech_StencilShadowVolumes;
                        svMethod   = eSVMethod_DepthFail;
                    } else if (strcmp(optionArg, "ssvdp") == 0) {
                        shadowTech = eShadowTech_StencilShadowVolumes;
                        svMethod   = eSVMethod_SilhoutteDepthPass;
                    } else if (strcmp(optionArg, "ssvdf") == 0) {
                        shadowTech = eShadowTech_StencilShadowVolumes;
                        svMethod   = eSVMethod_SilhoutteDepthFail;
                    } else if (strcmp(optionArg, "sm") == 0) {
                        shadowTech = eShadowTech_ShadowMapping;
                    } else {
                        shadowTech = eShadowTech_None;
                    }
                }
            } else if (strcmp(option, "sm-resolution") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    smResolution = atoi(optionArg);
                }
            } else if (strcmp(option, "sm-bias-constant") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    smBiasConstant = atof(optionArg);
                }
            } else if (strcmp(option, "sm-bias-slope") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    smBiasSlope = atof(optionArg);
                }
            } else if (strcmp(option, "sm-z-near") == 0) {
                if (optionArg = fetch_option_arg(i, argc, argv)) {
                    smZNear = atof(optionArg);
                }
            }
        } else {
            valid = true;
            filename = arg;
        }
    }

    // Sanity checking
    if (width <= 0 || height <= 0) {
        valid = false;
    }
    if (test && (testFrames <= 0 || testTimeStep <= 0)) {
        valid = false;
    }
    if (shadowTech == eShadowTech_ShadowMapping && smResolution <= 0) {
        valid = false;
    }
}

void Configuration::printUsage(char* argv0) {
    std::cout << std::format("Usage: {} [options] <gltf/glb file>\n", argv0);
    std::cout << "Available options:\n";
    std::cout << "    --help                        Display this help message.\n";
    std::cout << "    --resizable / --no-resizable  Makes the window resizable or static (default: resizable)\n";
    std::cout << "    --vsync / --no-vsync          Enables/disables V-Sync (default: disabled)\n";
    std::cout << "    --width <integer>             Specifies the width of the window (default: 1280)\n";
    std::cout << "    --height <integer>            Specifies the height of the window (default: 720)\n";
    std::cout << "    --gpu-index <integer>         Specifies which GPU to use, follows order given by Vulkan (default: any)\n";
    std::cout << "    --test / --no-test            Enables/disables test mode (default: disabled)\n";
    std::cout << "    --test-frames <integer>       Specifies length of the test in frames (default: 300)\n";
    std::cout << "    --test-timestep <seconds>     Specifies animation timestep in test mode (default: 16.666ms)\n";
    std::cout << "    --shadow-tech <name>          Specifies the shadow technique to use (default: none),\n";
    std::cout << "                                  Available variants:\n";
    std::cout << "                                      svdp  - Shadow Volumes Depth Pass\n";
    std::cout << "                                      svdf  - Shadow Volumes Depth Fail\n";
    std::cout << "                                      ssvdp - Silhoutte Shadow Volumes Depth Pass\n";
    std::cout << "                                      ssvdf - Silhoutte Shadow Volumes Depth Fail\n";
    std::cout << "                                      sm    - Shadow Mapping\n";
    std::cout << "\n";
    std::cout << "Shadow mapping options:\n";
    std::cout << "    --sm-resolution    <integer>           Specifies the resolution of the shadow map (default: 512)\n";
    std::cout << "    --sm-bias-constant <number>            Specifies the constant factor for shadow map bias (default: 512).\n";
    std::cout << "    --sm-bias-slope    <number>            Specifies the slope factor for shadow map bias (default: 4).\n";
    std::cout << "    --sm-z-near        <number>            Specifies the z-near coordinate for shadow maps (default: 0.1).\n";
    std::cout << "    --sm-pcf / --no-sm-pcf                 Enables/disables usage of a 2x2 HW PCF sampler in shadow mapping (default: enabled).\n";
    std::cout << "    --sm-cull-front / --no-sm-cull-front   Enables/disables culling of front faces in shadow maps (default: enabled).\n";
    std::cout << "\n";
    std::cout << "Light options:\n";
    std::cout << "    --light-ignore-node             App will ignore light nodes present in the scene.\n";
    std::cout << "    --light-position  <x> <y> <z>   Specifies the starting position of the light source (default: 0 0 0).\n";
    std::cout << "    --light-ambient   <r> <g> <b>   Specifies the ambient color of the light source (default: 0.3 0.3 0.5).\n";
    std::cout << "    --light-diffuse   <r> <g> <b>   Specifies the diffuse color of the light source (default: 0.7 0.5 0.5).\n";
    std::cout << "    --light-range     <number>      Specifies the range of the light source (default: 100).\n";
    std::cout << "    --light-intensity <number>      Specifies the intensity of the light source (default: 2).\n";
    std::cout << "\n";
    std::cout << "Camera options:\n";
    std::cout << "    --camera-ignore-node          App will ignore camera nodes present in the scene.\n";
    std::cout << "    --camera-eye    <x> <y> <z>   Specifies the camera eye position (default: 0 1 0).\n";
    std::cout << "    --camera-target <x> <y> <z>   Specifies the camera target position (default: 0 1 1).\n";
    std::cout << "    --camera-z-near <number>      Specifies the z-near coordinate for the camera (default: 0.25).\n";
    std::cout << "    --camera-z-far  <number>      Specifies the z-far coordinate for the camera (default: 1000).\n";
    std::cout << "    --camera-fov    <number>      Specifies the camera field of view in degrees (default: 45).\n";
}