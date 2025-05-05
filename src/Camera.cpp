/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Camera controller class.
///
#include "Camera.hpp"
#include "Scene.hpp"

void Camera::updateControlled(float deltaTime, const Uint8* kb, float xrel, float yrel) {
    moveSpeed = kb[SDL_SCANCODE_LSHIFT] ? 20 : 5;
    
    yaw   -= xrel * rotateSpeed;
    pitch += yrel * rotateSpeed;

    constexpr float minPitch = -glm::pi<float>()/2 + glm::pi<float>()/16;
    constexpr float maxPitch =  glm::pi<float>()/2 - glm::pi<float>()/16;
    pitch = glm::clamp(pitch, minPitch, maxPitch);

    glm::mat4 rotation = glm::rotate(glm::mat4(1.0), yaw,   {0, 1, 0})
                       * glm::rotate(glm::mat4(1.0), pitch, {1, 0, 0});

    glm::vec3 dir  = glm::vec3(rotation * glm::vec4(0,0,1,1));
    glm::vec3 left = glm::vec3(rotation * glm::vec4(1,0,0,1));
    
    if (kb[SDL_SCANCODE_W]) {
        eye += dir * moveSpeed * deltaTime;
    }
    if (kb[SDL_SCANCODE_S]) {
        eye -= dir * moveSpeed * deltaTime;
    }
    if (kb[SDL_SCANCODE_A]) {
        eye += left * moveSpeed * deltaTime;
    }
    if (kb[SDL_SCANCODE_D]) {
        eye -= left * moveSpeed * deltaTime;
    }
    target = eye + dir;
}

void Camera::fromTransformMatrix(const glm::mat4& matrix) {
    eye    = glm::vec3(matrix[3]);
    target = glm::vec3(matrix * glm::vec4(0,0,-1,1));
}

void Camera::copyToSceneCameraBuffer(Scene& scene) {
    view       = glm::lookAt(eye, target, { 0, 1, 0 });
    projection = glm::perspective(fov, aspectRatio, depthNear, depthFar);

    scene.camera.eye         = eye;
    scene.camera.target      = target;
    scene.camera.fov         = fov;
    scene.camera.aspectRatio = aspectRatio;
    scene.camera.depthNear   = depthFar;
    scene.camera.projection  = projection;
    scene.camera.view        = view;

    // Flip Y axis. GLM produces a projection matrix where Y=-1 is the top
    // and Y=1 is the bottom, while Vulkan's NDC space is the opposite way.
    scene.camera.projection[1][1] *= -1;
    scene.camera.projView = scene.camera.projection
                          * scene.camera.view;
}