/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Entry point, main loop.
///
#include <iostream>
#include <fstream>
#include <vector>
#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>
#include <ImGuizmo.h>
#include "Common.hpp"
#include "Renderer.hpp"
#include "Swapchain.hpp"
#include "Scene.hpp"
#include "Camera.hpp"
#include "BindlessSet.hpp"
#include "CommonSamplers.hpp"
#include "ScenePipelines.hpp"
#include "Configuration.hpp"
#include "glm.hpp"

static const char* shadow_tech_names[] = {
    "No Shadows",
    "Shadow Mapping",
    "Stencil Shadow Volumes"
};

static const char* sv_method_names[] = {
    "Depth Pass",
    "Depth Fail",
    "Silhoutte Depth Pass",
    "Silhoutte Depth Fail",
};

static const uint32_t shadow_map_resolutions[] = {
    128, 256, 512, 1024, 2048, 4096, 8192
};

static const char* shadow_map_resolution_names[] = {
    "128x128",
    "256x256",
    "512x512",
    "1024x1024",
    "2048x2048",
    "4096x4096",
    "8192x8192"
};

/// Special class to ensure correct initialization and deinitialization
/// order with C++'s RAII.
class SDLInitObj {
public:
    SDLInitObj() {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
            throw std::runtime_error("Could not initialize SDL.");
        }
        if (SDL_Vulkan_LoadLibrary(nullptr)) {
            throw std::runtime_error("SDL could not load the Vulkan library.");
        }
    }
    ~SDLInitObj() {
        SDL_Vulkan_UnloadLibrary();
        SDL_Quit();
    }
};


static void init_imgui(Renderer& renderer, VkDescriptorPool pool) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL2_InitForVulkan(renderer.getWindow());
    ImGui_ImplVulkan_InitInfo ii = {};
    ii.Instance       = renderer.getInstance();
    ii.PhysicalDevice = renderer.getPhysDevice();
    ii.Device         = renderer.getDevice();
    ii.QueueFamily    = renderer.getQueueFamily();
    ii.Queue          = renderer.getQueue();
    ii.DescriptorPool = pool;
    ii.RenderPass     = renderer.getSwapchain().getRenderPass();
    ii.Subpass        = 0;
    ii.MinImageCount  = renderer.getSwapchain().getImageCount();
    ii.ImageCount     = ii.MinImageCount;
    ii.MSAASamples    = VK_SAMPLE_COUNT_1_BIT;
    ImGui_ImplVulkan_Init(&ii);
}

static VkDescriptorPool create_imgui_descpool(Renderer& renderer) {
    VkDescriptorPoolSize poolSizes[1] = {
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
    };

    VkDescriptorPoolCreateInfo dpci = { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    dpci.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    dpci.poolSizeCount = ARRAY_COUNT(poolSizes);
    dpci.pPoolSizes    = poolSizes;
    dpci.maxSets       = 1;

    VkDescriptorPool pool;
    VKCHECK(vkCreateDescriptorPool(renderer.getDevice(), &dpci, nullptr, &pool));
    return pool;
}

///
/// The program's entry point.
///
int main(int argc, char* argv[]) {
    SDLInitObj __sdl;

    Configuration conf(argc, argv);
    if (conf.help || !conf.valid) {
        Configuration::printUsage(argv[0]);
        return 0;
    }

    GfxSettings gfxsettings = {
        .width          = conf.width,
        .height         = conf.height,
        .vsync          = conf.vsync,
        .resizable      = conf.resizable,
        .gpuIndex       = conf.gpuIndex,
        .needTimestamps = conf.test,
    };
    Renderer renderer("Vulkan Shadows", gfxsettings);

    VkDescriptorPool imguiDescriptorPool = create_imgui_descpool(renderer);
    init_imgui(renderer, imguiDescriptorPool);

    CommonSamplers samplers(renderer);
    BindlessSet    bindlessSet(renderer);
    ScenePipelines scenePipelines(renderer, bindlessSet.getLayout());
    bindlessSet.setSamplerIndex(eSampler_Linear,  samplers.linear);
    bindlessSet.setSamplerIndex(eSampler_Nearest, samplers.nearest);

    Scene scene(renderer, scenePipelines, conf.filename);

    Scene::LightData startLight = {
        .position  = conf.lightPosition,
        .intensity = conf.lightIntensity,
        .ambient   = conf.lightAmbient,
        .range     = conf.lightRange,
        .diffuse   = conf.lightDiffuse
    };

    if (!conf.lightIgnoreNode && scene.lightNodeID >= 0) {
        // Extract the light position from the scene if it's present.
        startLight.position = glm::vec3(scene.getNodeTransform(scene.lightNodeID)[3]);
    }
    scene.lights.push_back(startLight);

    Camera camera = {
        .moveSpeed   = 1.0f,
        .rotateSpeed = 0.01f,
        .depthNear   = conf.cameraZNear,
        .depthFar    = conf.cameraZFar,
        .fov         = glm::radians(conf.cameraFov),
        .eye         = conf.cameraEye,
        .target      = conf.cameraTarget,
    };

    if (!conf.cameraIgnoreNode && scene.cameraNodeID >= 0) {
        // Place camera in place of the node in the scene if there is one.
        camera.fromTransformMatrix(scene.getNodeTransform(scene.cameraNodeID));
    }

    int   resolutionSelection = 2; // 512
    bool  showUI              = !conf.test;
    bool  mouseCaptured       = false;
    bool  followCameraNode    = conf.test;
    bool  followLightNode     = !conf.lightIgnoreNode;

    const Uint8* keyboard = SDL_GetKeyboardState(nullptr);

    SDL_Event e;
    bool running = true;

    int    frames    = 0;
    float  deltaTime = 0;
    Uint64 pcLast    = 0;
    Uint64 pcNow     = SDL_GetPerformanceCounter();

    ImGuiIO& io = ImGui::GetIO();
    while (running) {
        if (conf.test && frames >= conf.testFrames) {
            // Automatically end the program after the test has finished.
            running = false;
            break;
        }

        Sint32 mouseXRel = 0;
        Sint32 mouseYRel = 0;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            case SDL_WINDOWEVENT:
                switch (e.window.event) {
                case SDL_WINDOWEVENT_RESIZED: renderer.getSwapchain().markAsOutdated(); break;
                case SDL_WINDOWEVENT_CLOSE:   running = false; break;
                default: break;
                }
                break;
            }

            if (!conf.test && e.type == SDL_KEYDOWN && e.key.keysym.scancode == SDL_SCANCODE_F12) {
                showUI = !showUI;
            }
            
            if (showUI) {
                ImGui_ImplSDL2_ProcessEvent(&e);
                // Don't process input if imgui wants it.
                if (io.WantCaptureMouse || io.WantCaptureKeyboard) {
                    continue;
                }
            }

            switch (e.type) {
            case SDL_KEYDOWN:
                if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    mouseCaptured = false;
                    SDL_SetRelativeMouseMode(SDL_FALSE);
                    io.ConfigFlags &= ~(ImGuiConfigFlags_NoMouse|ImGuiConfigFlags_NoKeyboard);
                }
                break;
            case SDL_MOUSEBUTTONDOWN:
                mouseCaptured = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                io.ConfigFlags |= (ImGuiConfigFlags_NoMouse|ImGuiConfigFlags_NoKeyboard);
                break;
            case SDL_MOUSEMOTION:
                if (mouseCaptured) {
                    mouseXRel += e.motion.xrel;
                    mouseYRel += e.motion.yrel;
                }
                break;
            }
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGuizmo::BeginFrame();

        VkExtent2D extent = renderer.getSwapchain().getExtent();
        
        camera.aspectRatio = float(extent.width) / float(extent.height);
        if (followCameraNode && scene.cameraNodeID >= 0) {
            camera.fromTransformMatrix(scene.getNodeTransform(scene.cameraNodeID));
        } else if (mouseCaptured) {
            camera.updateControlled(deltaTime, keyboard, mouseXRel, mouseYRel);
        }
        camera.copyToSceneCameraBuffer(scene);
        
        if (showUI) {
            ImGui::Begin("Inspector");
            ImGui::Text("Eye   : (%g, %g, %g)",
                        camera.eye.x,
                        camera.eye.y,
                        camera.eye.z);
            ImGui::Text("Target: (%g, %g, %g)",
                        camera.target.x,
                        camera.target.y,
                        camera.target.z);
            ImGui::Separator();

            if (scene.lightNodeID > 0) {
                ImGui::Checkbox("Follow scene's light node", &followLightNode);
            } else {
                followLightNode = false;
                ImGui::Text("No light node in the scene.");
            }

            if (scene.cameraNodeID > 0) {
                ImGui::Checkbox("Follow scene's camera node", &followCameraNode);
            } else {
                followCameraNode = false;
                ImGui::Text("No camera node in the scene.");
            }

            ImGui::Separator();
            ImGui::Combo("Shadowing Technique", (int*) &conf.shadowTech, shadow_tech_names, ARRAY_COUNT(shadow_tech_names));
            ImGui::Separator();

            switch (conf.shadowTech) {
            case eShadowTech_None:
                ImGui::Text("No options for shadowless mode.");
                break;
            case eShadowTech_ShadowMapping:
                ImGui::Text("Shadow mapping configuration:");
                ImGui::Text("Used VkFormat for the shadow map: %s", string_VkFormat(renderer.getBestDepthFormat()));
                ImGui::Combo("Shadow Map Resolution", &resolutionSelection, shadow_map_resolution_names, ARRAY_COUNT(shadow_map_resolution_names));
                ImGui::Checkbox("Cull front faces in shadow maps when applicable", &conf.smCullFrontFaces);
                ImGui::InputFloat("Depth Bias Constant Factor", &conf.smBiasConstant, 0, 0, "%.8f");
                ImGui::InputFloat("Depth Bias Slope Factor", &conf.smBiasSlope, 0, 0, "%.8f");
                ImGui::DragFloat("Depth Near", &conf.smZNear, 0.001f);
                ImGui::Checkbox("Use PCF shadow sampler", &conf.smPCFSampler);

                // Handle switching shadow map resolutions by deleting the
                // previous shadow map textures and making the Scene class
                // allocate a new one.
                if (shadow_map_resolutions[resolutionSelection] != conf.smResolution) {
                    conf.smResolution = shadow_map_resolutions[resolutionSelection];
                    renderer.waitForDevice();
                    scene.shadowMaps.clear();
                }
                break;
            case eShadowTech_StencilShadowVolumes:
                ImGui::Combo("SV Method", (int*) &conf.svMethod, sv_method_names, ARRAY_COUNT(sv_method_names));
                ImGui::Checkbox("Silhoutte Debug Overlay", &conf.svDebugOverlay);
                break;
            }

            ImGui::Separator();
            ImGui::InputFloat3("Light Position", &scene.lights[0].position.x);
            ImGui::ColorEdit3("Ambient Color",   &scene.lights[0].ambient.x);
            ImGui::ColorEdit3("Diffuse Color",   &scene.lights[0].diffuse.x);
            ImGui::DragFloat("Range",            &scene.lights[0].range,     0.1f);
            ImGui::DragFloat("Intensity",        &scene.lights[0].intensity, 0.1f);
            ImGui::End();

            // Display the guizmo for the light if we have control over it.
            if (!followLightNode) {
                ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
                glm::mat4 lightMatrix = glm::translate(glm::mat4(1), scene.lights[0].position);
                ImGuizmo::Manipulate(&camera.view[0][0],
                                     &camera.projection[0][0],
                                     ImGuizmo::TRANSLATE,
                                     ImGuizmo::WORLD,
                                     &lightMatrix[0][0]);
                scene.lights[0].position = glm::vec3(lightMatrix[3]);
            }
        }

        scene.shadowMapConf = {
            .resolution     = uint32_t(conf.smResolution),
            .cullFrontFaces = conf.smCullFrontFaces,
            .biasConstant   = conf.smBiasConstant,
            .biasSlope      = conf.smBiasSlope,
            .zNear          = conf.smZNear
        };

        if (followLightNode && scene.lightNodeID >= 0) {
            scene.lights[0].position = glm::vec3(scene.getNodeTransform(scene.lightNodeID)[3]); // Extract position
        }

        renderer.getSwapchain().recordFrame([&](Swapchain& swapchain, VkCommandBuffer cmdbuf) {
            const auto vkset = bindlessSet.getSet();
            vkCmdBindDescriptorSets(cmdbuf, VK_PIPELINE_BIND_POINT_GRAPHICS, scenePipelines.layout, 0, 1, &vkset, 0, nullptr);
            bindlessSet.clearImageViews();
            if (conf.smPCFSampler) {
                bindlessSet.setSamplerIndex(eSampler_Shadow, samplers.shadowLinear);
            } else {
                bindlessSet.setSamplerIndex(eSampler_Shadow, samplers.shadow);
            }
            scene.fillOutBindlessSet(bindlessSet);

            // Shadow Map render pass if necessary.
            if (conf.shadowTech == eShadowTech_ShadowMapping) {
                scene.recordDrawBufferUpdates(cmdbuf);
                scene.drawToShadowMaps(cmdbuf, bindlessSet);
            }
            scene.recordDrawBufferUpdates(cmdbuf);

            // Swapchain renderpass
            swapchain.beginRenderPass();
            swapchain.setDefaultViewportScissor();

            switch (conf.shadowTech) {
            case eShadowTech_None:
                scene.recordScene(cmdbuf);
                break;
            case eShadowTech_ShadowMapping:
                scene.recordScene(cmdbuf, eScenePipelineFlags_Depth, eSceneDrawType_ShadowMapped);
                break;
            case eShadowTech_StencilShadowVolumes:
                scene.recordScene(cmdbuf, eScenePipelineFlags_Depth, eSceneDrawType_Ambient);
                scene.recordShadowVolumesStencil(cmdbuf, conf.svMethod, 0);
                scene.recordScene(cmdbuf, 0, eSceneDrawType_DiffuseStencilTested);
                if (conf.svDebugOverlay) {
                    scene.recordSilhoutteDebugOverlay(cmdbuf, 0);
                }
                break;
            }
            
            ImGui::Render();
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdbuf);
            vkCmdEndRenderPass(cmdbuf);
        });
        scene.advanceAnimations(conf.test ? conf.testTimeStep : deltaTime, true);
        
        // Adjust deltaTime.
        pcLast    = pcNow;
        pcNow     = SDL_GetPerformanceCounter();
        deltaTime = (pcNow - pcLast) / float(SDL_GetPerformanceFrequency());
        frames++;
    }

    // Wait so that we don't start deleting
    // resources the GPU might still be using.
    renderer.waitForDevice();

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (imguiDescriptorPool) {
        vkDestroyDescriptorPool(renderer.getDevice(), imguiDescriptorPool, nullptr);
    }

    return 0;
}