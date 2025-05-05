/// 
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Class for creating vertex, index, and edge index buffers for a given glTF
/// model.
/// 
/// Edge + opposite vertices are encoded in the edge index buffer as
/// a "triangle list with adjacency" primitive that allows to specify up to 6
/// total vertices. So, 2 vertices for the edge itself and up to 4 opposite
/// vertices. Unused opposite vertex slots are encoded as repeats of vertex 0
/// (first edge vertex).
/// Intended for access from the geometry shader.
///
#pragma once

#include <stdexcept>
#include "tiny_gltf_wrap.hpp"
#include "GpuBuffer.hpp"
#include "Vertex.hpp"

struct VIBPrimGroup {
    uint32_t materialID;
    int32_t  vertexOffset;    // in bytes
    int32_t  indexOffset;     // in bytes
    int32_t  edgeIndexOffset; // in bytes
    uint32_t vertexCount;
    uint32_t indexCount;
    uint32_t edgeIndexCount;
};

// Once std::inplace_vector becomes widely available with C++26, this structure
// can be cut altogether.
template<size_t max>
struct OppositeVertices {
    OppositeVertices() : count(0), data{} {}
    ~OppositeVertices() = default;
    void push_back(uint32_t index) {
        if (count >= max) {
            throw std::runtime_error("Too many opposite vertices.");
        }
        data[count++] = index;
    }
    void clear() {
        count = 0;
    }
    uint32_t operator[](uint32_t index) const {
        return data[index];
    }
    uint32_t  size()  const { return count;        }
    uint32_t* begin()       { return &data[0];     }
    uint32_t* end()         { return &data[count]; }

    uint32_t count;
    uint32_t data[max];
};

union Edge {
    Edge(uint32_t f, uint32_t s)
        : first(std::min(f, s)), second(std::max(f, s))
    {}

    uint32_t indices[2];
    struct { uint32_t first, second; };
    bool operator==(const Edge& other) const {
        return (first == other.first  && second == other.second)
            || (first == other.second && second == other.first);
    }
};

template<>
struct std::hash<Edge> {
    std::size_t operator()(const Edge& e) const {
        // Keep commutative.
        // Hash combination function taken from the boost library.
        // (See: https://www.boost.org/doc/libs/1_43_0/doc/html/hash/reference.html#boost.hash_combine)
        uint32_t first  = std::min(e.first, e.second);
        uint32_t second = std::max(e.first, e.second);
        return first ^ (second + 0x9e3779b9 + (first << 6) + (first >> 2));
    }
};

/// Generates Vertex/Index buffer from a supplied tinygtlf mesh.
class VIBufferBuilder {
public:
    VIBufferBuilder(Renderer& renderer, const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh);
    VIBufferBuilder(VIBufferBuilder&&)      = delete;
    VIBufferBuilder(const VIBufferBuilder&) = delete;

    GpuVertexIndexBuffer create();

    std::vector<VIBPrimGroup> groups; // Public to be able to be moved by the user.
private:
    uint32_t calcStagingSize(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh);
    void     readVertices(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh);
    void     readIndices(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh);
    void     findEdges();
    void     unpackEdges();

    // Limiting ourselves to a maximum amount of opposite vertices
    // to avoid an overhead of having to manage dynamic arrays for each edge.
    // The maximum we can pass to the geometry shader via the "triangle with adjacency"
    // primitive is 4, hence the maximum amount here.
    std::vector<std::unordered_map<Edge, OppositeVertices<4>>> edges;

    Renderer&        renderer;
    uint32_t         totalVertexCount;
    uint32_t         totalIndexCount;
    uint32_t         totalEdgeIndexCount;
    uint32_t         vbOffset, ibOffset, ebOffset;
    uint32_t         vbSize, ibSize, ebSize;
    uint32_t         stagingBufferSize;
    GpuStagingBuffer staging;
    uint8_t*         mappedStaging;
    VertexNT*        stgVertices;
    uint32_t*        stgIndices;
};
