#ifndef _DX12_RENDER_MESH_H_
#define _DX12_RENDER_MESH_H_

#include "Common.h"
#include "Common_DX12.h"

struct DX12RenderMesh {
    inline D3D12_VERTEX_BUFFER_VIEW VertexBufferView() const {
        D3D12_VERTEX_BUFFER_VIEW result;

        result.BufferLocation = vertexBufferGPU->GetGPUVirtualAddress();
        result.StrideInBytes = vertexByteStride;
        result.SizeInBytes = vertexBufferByteSize;

        return result;
    }

    inline D3D12_INDEX_BUFFER_VIEW IndexBufferView() const {
        D3D12_INDEX_BUFFER_VIEW result;

        result.BufferLocation = indexBufferGPU->GetGPUVirtualAddress();
        result.Format = indexFormat;
        result.SizeInBytes = indexBufferByteSize;

        return result;
    }

    D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT                     indexCount;
    UINT                     startIndexLoc;
    int                      baseVertexLoc;

    ID3DBlob       *vertexBufferCPU;
    ID3D12Resource *vertexBufferGPU;
    ID3D12Resource *vertexBufferUploader;

    ID3DBlob       *indexBufferCPU;
    ID3D12Resource *indexBufferGPU;
    ID3D12Resource *indexBufferUploader;

    UINT        vertexByteStride;
    UINT        vertexBufferByteSize;
    DXGI_FORMAT indexFormat;
    UINT        indexBufferByteSize;
};

#endif //!_DX12_RENDER_MESH_H_