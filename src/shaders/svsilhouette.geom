/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Geometry-shader implementation of the silhouette evaluation algorithm
/// described in
/// 
/// PEČIVA, J.; STARKA, T.; MILET, T.; KOBRTEK, J. and ZEMČÍK, P.
/// Robust Silhouette Shadow Volumes on Contemporary Hardware
/// https://www.fit.vut.cz/research/publication/10408
///

#version 450

#include "scene.glsl"

// NOTE:
// vertex 0 - edge vertex 0
// vertex 1 - edge vertex 1
// vertex 2 - opposite vertex 0
// vertex 3 - opposite vertex 1
// vertex 4 - opposite vertex 2
// vertex 5 - opposite vertex 3
// If an opposite vertex is the same as the first edge vertex, then that
// opposite vertex does not exist.
// Index buffer for this shader is generated in the VIBufferBuilder class.
layout (triangles_adjacency) in;
layout (location = 0) in vec4 worldPos[];

#ifdef DEBUG
layout (line_strip, max_vertices = 2) out;
layout (location = 0) out vec4 outColor;
#else
layout (triangle_strip, max_vertices = 16) out;
#endif

int edge_multiplicity(vec4 a, vec4 b, vec4 ov, vec3 l) {
    const vec3 n  = normalize(cross(a.xyz - b.xyz, l - a.xyz));
    const vec4 lp = vec4(n, -dot(n, a.xyz)); // light plane
    return int(sign(dot(lp, ov)));
}

int eval_triangle(vec4 a, vec4 b, vec4 c, vec3 l) {
    if (c == a) {
        return 0;
    } else {
        // Aggressive inconsistency detection.
        // Check if the edge multiplicities of the 
        // triangle are consistent, as well as checking 
        // whether the triangle with the opposite winding
        // has consistent multiplicities AND whether
        // the difference in winding results in a different
        // multiplicity sign.
        int e0 = edge_multiplicity(a, b, c, l);
        int e1 = edge_multiplicity(b, c, a, l);
        int e2 = edge_multiplicity(c, a, b, l);

        int e3 = edge_multiplicity(c, b, a, l);
        int e4 = edge_multiplicity(a, c, b, l);
        int e5 = edge_multiplicity(b, a, c, l);

        int sum0 = e0 + e1 + e2;
        int sum1 = e3 + e4 + e5;
        if (abs(sum0) == 3 && abs(sum1) == 3 && sign(sum0) != sign(sum1)) {
            return e0;
        } else {
            return 0;
        }
    }   
}

void main() {
    const Light light = lights.data[currentLightID];

    const vec3 l  = light.position;
    const vec4 f0 = gl_in[0].gl_Position;
    const vec4 f1 = gl_in[1].gl_Position;
    const vec3 d0 = normalize(worldPos[0].xyz - l);
    const vec3 d1 = normalize(worldPos[1].xyz - l);
    const vec4 o0 = camera.projView * vec4(d0, 0);
    const vec4 o1 = camera.projView * vec4(d1, 0);
    const vec4 a  = worldPos[0];
    const vec4 b  = worldPos[1];
    int multiplicity = 0;
    multiplicity += eval_triangle(a, b, worldPos[2], l);
    multiplicity += eval_triangle(a, b, worldPos[3], l);
    multiplicity += eval_triangle(a, b, worldPos[4], l);
    multiplicity += eval_triangle(a, b, worldPos[5], l);

#ifndef DEBUG
    for (int i = 0; i < abs(multiplicity); ++i) {
        if (multiplicity >= 0) {
            gl_Position = f0; EmitVertex();
            gl_Position = o0; EmitVertex();
            gl_Position = f1; EmitVertex();
            gl_Position = o1; EmitVertex();
            EndPrimitive();
        } else {
            gl_Position = o0; EmitVertex();
            gl_Position = f0; EmitVertex();
            gl_Position = o1; EmitVertex();
            gl_Position = f1; EmitVertex();
            EndPrimitive();
        }
    }
#else
    // Draw debug edges
    vec4 v0 = gl_in[0].gl_Position;
    vec4 v1 = gl_in[1].gl_Position;
    v0.z -= 0.0001;
    v1.z -= 0.0001;
    outColor = multiplicity != 0 ? vec4(1,1,0,1) : vec4(1,0,0,1);
    gl_Position = v0; EmitVertex();
    outColor = multiplicity != 0 ? vec4(1,1,0,1) : vec4(1,0,0,1);
    gl_Position = v1; EmitVertex();
    EndPrimitive();
#endif
}