///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Geometry shader for generating per-triangle shadow volumes.
///
#version 450
#include "scene.glsl"

layout (constant_id = 0) const uint FRONT_CAP = 0U;
layout (constant_id = 1) const uint BACK_CAP  = 0U;
layout (constant_id = 2) const uint SIDES     = 0U;

layout (triangles) in;
layout (triangle_strip, max_vertices = 14) out;

layout (location = 0) in vec4 worldPos[];

void main() {
    const Light light = lights.data[currentLightID];
    
    // NOTE: keep the order of vertex transformations consistent with
    // scene.vert, as even the tiniest difference can cause loss of precision
    // and possible self-shadowing artifacts.

    const vec4 f0 = gl_in[0].gl_Position;
    const vec4 f1 = gl_in[1].gl_Position;
    const vec4 f2 = gl_in[2].gl_Position;

    const vec3 d0 = normalize(worldPos[0].xyz - light.position);
    const vec3 d1 = normalize(worldPos[1].xyz - light.position);
    const vec3 d2 = normalize(worldPos[2].xyz - light.position);

    const vec4 o0 = camera.projView * vec4(d0, 0);
    const vec4 o1 = camera.projView * vec4(d1, 0);
    const vec4 o2 = camera.projView * vec4(d2, 0);

    const vec3 e0 = worldPos[1].xyz - worldPos[0].xyz;
    const vec3 e1 = worldPos[2].xyz - worldPos[1].xyz;
    if (dot(cross(e0, e1), (d0 + d1 + d2) / 3) < 0) {
        // Triangle faces the light.
        if (SIDES > 0) {
            gl_Position = f0; EmitVertex();
            gl_Position = o0; EmitVertex();
            gl_Position = f1; EmitVertex();
            gl_Position = o1; EmitVertex();
            gl_Position = f2; EmitVertex();
            gl_Position = o2; EmitVertex();
            gl_Position = f0; EmitVertex();
            gl_Position = o0; EmitVertex();
            EndPrimitive();
        }
        if (FRONT_CAP > 0) {
            gl_Position = f0; EmitVertex();
            gl_Position = f1; EmitVertex();
            gl_Position = f2; EmitVertex();
            EndPrimitive();
        }
        if (BACK_CAP > 0) {
            gl_Position = o2; EmitVertex();
            gl_Position = o1; EmitVertex();
            gl_Position = o0; EmitVertex();
            EndPrimitive();
        }
    } else {
        // Invert the winding of the generated shadow volume
        // for triangles that face away from the light.
        if (SIDES > 0) {
            gl_Position = o0; EmitVertex();
            gl_Position = f0; EmitVertex();
            gl_Position = o1; EmitVertex();
            gl_Position = f1; EmitVertex();
            gl_Position = o2; EmitVertex();
            gl_Position = f2; EmitVertex();
            gl_Position = o0; EmitVertex();
            gl_Position = f0; EmitVertex();
            EndPrimitive();
        }
        if (FRONT_CAP > 0) {
            gl_Position = f2; EmitVertex();
            gl_Position = f1; EmitVertex();
            gl_Position = f0; EmitVertex();
            EndPrimitive();
        }
        if (BACK_CAP > 0) {
            gl_Position = o0; EmitVertex();
            gl_Position = o1; EmitVertex();
            gl_Position = o2; EmitVertex();
            EndPrimitive();
        }
    }
}
