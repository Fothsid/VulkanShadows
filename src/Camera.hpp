/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Camera controller class.
///
#pragma once
#include <SDL2/SDL.h>
#include "glm.hpp"

class Scene; // Forward declaration
struct Camera {
    void updateControlled(float deltaTime, const Uint8* kb, float xrel, float yrel);
    void fromTransformMatrix(const glm::mat4& matrix);
    void copyToSceneCameraBuffer(Scene& scene);

    float     moveSpeed;
    float     rotateSpeed;
    float     depthNear;
    float     depthFar;
    float     fov;
    glm::vec3 eye;
    glm::vec3 target;
    
    glm::mat4 projection;
    glm::mat4 view;
    float     aspectRatio;
    float     pitch;
    float     yaw;
};