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
#include <stdexcept>
#include <iostream>
#include "VIBufferBuilder.hpp"

VIBufferBuilder::VIBufferBuilder(Renderer& renderer, const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh)
    : renderer(renderer)
    , stagingBufferSize(calcStagingSize(gltfModel, gltfMesh))
    , staging(renderer, stagingBufferSize)
    , mappedStaging(reinterpret_cast<uint8_t*>(staging.getMappedData()))
    , stgVertices(reinterpret_cast<VertexNT*>(mappedStaging + vbOffset))
    , stgIndices(reinterpret_cast<uint32_t*>(mappedStaging + ibOffset))
{
    readVertices(gltfModel, gltfMesh);
    readIndices(gltfModel, gltfMesh);
    findEdges();
    unpackEdges();
}

GpuVertexIndexBuffer VIBufferBuilder::create() {
    GpuVertexIndexBuffer out(renderer, stagingBufferSize);
    renderer.recordOneTime([&](VkCommandBuffer cmdbuf) {
        out.copyFrom(cmdbuf, staging, stagingBufferSize);
    });
    return out;
}

uint32_t VIBufferBuilder::calcStagingSize(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh) {
    totalVertexCount = 0;
    totalIndexCount  = 0;
    for (const auto& gltfGroup : gltfMesh.primitives) {
        int pAccessor = gltfGroup.attributes.at("POSITION");
        int iAccessor = gltfGroup.indices;
        totalVertexCount += gltfModel.accessors[pAccessor].count;
        totalIndexCount  += gltfModel.accessors[iAccessor].count;
    }

    // 2 indices for the edge itself and 4 for the opposite vertices
    totalEdgeIndexCount = totalIndexCount * 6;

    vbSize = totalVertexCount    * sizeof(VertexNT);
    ibSize = totalIndexCount     * sizeof(uint32_t);
    ebSize = totalEdgeIndexCount * sizeof(uint32_t);

    vbOffset = 0;
    ibOffset = vbOffset + vbSize;
    ebOffset = ibOffset + ibSize;

    return vbSize + ibSize + ebSize;
}

void VIBufferBuilder::readVertices(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh) {
    uint32_t vpos = 0;
    for (const auto& gltfGroup : gltfMesh.primitives) {
        auto& group = groups.emplace_back();
        int pAccessorID = gltfGroup.attributes.at("POSITION");
        int nAccessorID = 0;
        int tAccessorID = 0;

        bool hasNormals   = gltfGroup.attributes.count("NORMAL");
        bool hasTexCoords = gltfGroup.attributes.count("TEXCOORD_0");

        if (hasNormals)   nAccessorID = gltfGroup.attributes.at("NORMAL");
        if (hasTexCoords) tAccessorID = gltfGroup.attributes.at("TEXCOORD_0");
        
        const auto& pAccessor = gltfModel.accessors[pAccessorID];
        const auto& nAccessor = gltfModel.accessors[nAccessorID];
        const auto& tAccessor = gltfModel.accessors[tAccessorID];

        const auto& pView = gltfModel.bufferViews[pAccessor.bufferView];
        const auto& nView = gltfModel.bufferViews[nAccessor.bufferView];
        const auto& tView = gltfModel.bufferViews[tAccessor.bufferView];

        const auto& pBuffer = gltfModel.buffers[pView.buffer];
        const auto& nBuffer = gltfModel.buffers[nView.buffer];
        const auto& tBuffer = gltfModel.buffers[tView.buffer];

        const auto pData = pBuffer.data.data() + pView.byteOffset + pAccessor.byteOffset;
        const auto nData = nBuffer.data.data() + nView.byteOffset + nAccessor.byteOffset;
        const auto tData = tBuffer.data.data() + tView.byteOffset + tAccessor.byteOffset;

        group.materialID   = uint32_t(gltfGroup.material);
        group.vertexOffset = vbOffset + vpos * sizeof(VertexNT);
        group.vertexCount  = pAccessor.count;

        uint32_t pStride = pView.byteStride;
        uint32_t nStride = nView.byteStride;
        uint32_t tStride = tView.byteStride;
        if (pStride == 0) pStride = sizeof(glm::vec3);
        if (nStride == 0) nStride = sizeof(glm::vec3);
        if (tStride == 0) tStride = sizeof(glm::vec2);
        for (uint32_t i = 0; i < group.vertexCount; ++i) {
            auto& vertex = stgVertices[vpos++];
            vertex.position = *(glm::vec3*)(pData + i * pStride);
            if (hasNormals)   { vertex.normal   = *(glm::vec3*)(nData + i * nStride); }
            if (hasTexCoords) { vertex.texCoord = *(glm::vec2*)(tData + i * tStride); }
        }
    }
}

void VIBufferBuilder::readIndices(const tinygltf::Model& gltfModel, const tinygltf::Mesh& gltfMesh) {
    uint32_t groupID = 0;
    uint32_t index = 0;
    for (const auto& gltfGroup : gltfMesh.primitives) {
        auto& group = groups[groupID++];
        int iAccessorID       = gltfGroup.indices;
        const auto& iAccessor = gltfModel.accessors[iAccessorID];
        const auto& iView     = gltfModel.bufferViews[iAccessor.bufferView];
        const auto& iBuffer   = gltfModel.buffers[iView.buffer];
        const auto  iData     = iBuffer.data.data() + iView.byteOffset + iAccessor.byteOffset;

        group.indexOffset = ibOffset + index * sizeof(uint32_t);
        group.indexCount  = iAccessor.count;

        switch (iAccessor.componentType) {
        default:
            throw std::runtime_error("Unsupported index buffer accessor type.");
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            for (uint32_t i = 0; i < iAccessor.count; ++i) {
                stgIndices[index++] = ((uint32_t*)iData)[i];
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            for (uint32_t i = 0; i < iAccessor.count; ++i) {
                stgIndices[index++] = ((uint16_t*)iData)[i];
            }
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            for (uint32_t i = 0; i < iAccessor.count; ++i) {
                stgIndices[index++] = ((uint8_t*)iData)[i];
            }
            break;
        }
    }
}

void VIBufferBuilder::findEdges() {
    edges.clear();
    edges.resize(groups.size());
    for (uint32_t g = 0; g < groups.size(); ++g) {
        auto& group = groups[g];
        const uint32_t* indices = reinterpret_cast<uint32_t*>(mappedStaging + group.indexOffset);

        // Reserve buckets to reduce the amount of rehashes. 
        edges[g].reserve(group.indexCount);

        for (uint32_t t = 0; t < group.indexCount; t += 3) {
            uint32_t i0 = indices[t+0];
            uint32_t i1 = indices[t+1];
            uint32_t i2 = indices[t+2];
            edges[g][Edge(i0, i1)].push_back(i2);
            edges[g][Edge(i1, i2)].push_back(i0);
            edges[g][Edge(i2, i0)].push_back(i1);
        }
    }
}

void VIBufferBuilder::unpackEdges() {
    uint32_t  written = 0;
    uint32_t* indices = reinterpret_cast<uint32_t*>(mappedStaging + ebOffset);
    for (uint32_t g = 0; g < groups.size(); ++g) {
        auto& group   = groups[g];
        auto& edgeMap = edges[g];

        uint32_t prevWritten  = written;
        group.edgeIndexOffset = ebOffset + written * sizeof(uint32_t);
        for (auto& pair : edgeMap) {
            const Edge& edge = pair.first;
            indices[written++] = edge.first;
            indices[written++] = edge.second;

            const auto& oppositeVertices = pair.second;
            for (uint32_t i = 0; i < 4; ++i) {
                indices[written++] = (i < oppositeVertices.size())
                                   ? oppositeVertices[i]
                                   : edge.first; // Put invalid opposite vertex if not present.
            }
        }
        group.edgeIndexCount = written - prevWritten;
    }
}