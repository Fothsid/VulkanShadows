/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Vertex structure definitions.
///
/// This was originally taken from an unpublished personal OpenGL project.
/// The final application effectively uses only VertexNT.
///
/// HOW THIS WORKS:
/// Each combination of eVertexFlags bits will describe a specific
/// Vertex* structure that are defined using macros below.
/// PipelineBuilder can then take that bitfield value and generate
/// appropriate vertex attributes.
///
/// The structure format can also be described as follows:
/// struct Vertex{N,}{T,}{C,} {
///     vec3 position;                     // location 0
///     if (flags & eVertexFlags_Normal)
///         vec3 normal;                   // location 1
///     if (flags & eVertexFlags_TexCoord)
///         vec2 texCoord;                 // location 2
///     if (flags & eVertexFlags_Colors)
///         vec4 color;                    // location 3
/// };
///
#pragma once

#include "glm.hpp"



enum eVertexFlags {
    eVertexFlags_None     = 0,
    eVertexFlags_Normal   = (1 << 0),
    eVertexFlags_TexCoord = (1 << 1),
    eVertexFlags_Color    = (1 << 2),
};

inline eVertexFlags operator|( eVertexFlags a, eVertexFlags b ) {
    return eVertexFlags(uint32_t(a)|uint32_t(b));
}

// Generating unique vertex structure types for every combination of 
// eVertexFlags using C preprocessor macros. It's honestly easier and more
// comprehensive to do it this way than with C++ templates.

#define VERTEX_TYPE(N,T,C) \
    struct {\
        glm::vec3 position; \
        N                   \
        T                   \
        C                   \
    }
#define N glm::vec3 normal;
#define T glm::vec2 texCoord;
#define C glm::vec4 color;

typedef VERTEX_TYPE( , , ) Vertex;    // position
typedef VERTEX_TYPE(N, , ) VertexN;   // position, normal
typedef VERTEX_TYPE( ,T, ) VertexT;   // position, texcoord
typedef VERTEX_TYPE(N,T, ) VertexNT;  // position, normal, texcoord
typedef VERTEX_TYPE( , ,C) VertexC;   // position, color
typedef VERTEX_TYPE(N, ,C) VertexNC;  // position, normal, color
typedef VERTEX_TYPE( ,T,C) VertexTC;  // position, texcoord, color
typedef VERTEX_TYPE(N,T,C) VertexNTC; // position, normal, texcoord, color

// Preprocessor cleanup
#undef N
#undef T
#undef C
#undef VERTEX_TYPE