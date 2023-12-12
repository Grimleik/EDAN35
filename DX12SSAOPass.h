#ifndef _DX12SSAO_PASS_H_
#define _DX12SSAO_PASS_H_

#include "Common_DX12.h"
#include "DX12CommandQueue.h"

struct SsaoConstants {
    DirectX::XMMATRIX   Proj;
    DirectX::XMMATRIX   InvProj;
    DirectX::XMFLOAT4X4 ProjTex;
    DirectX::XMFLOAT4   OffsetVectors[14];

    // For SsaoBlur.hlsl
    DirectX::XMFLOAT4 BlurWeights[3];

    DirectX::XMFLOAT2 InvRenderTargetSize = {0.0f, 0.0f};

    // Coordinates given in view space.
    float OcclusionRadius = 0.5f;
    float OcclusionFadeStart = 0.2f;
    float OcclusionFadeEnd = 2.0f;
    float SurfaceEpsilon = 0.05f;
};


struct DX12SSAOPass {
    DX12SSAOPass();
    ~DX12SSAOPass();

    void Initialize(ID3D12Device *device, ID3D12GraphicsCommandList2 *cmdList, UINT width, UINT height, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect);

    static const DXGI_FORMAT ambientMapFormat = DXGI_FORMAT_R16_UNORM;
    static const DXGI_FORMAT normalMapFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    static const int         maxBlurRadius = 5;

    UINT                          GetSSAOMapWidth() const;
    UINT                          GetSSAOMapHeight() const;
    void                          GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]);
    std::vector<float>            CalcGaussWeights(float sigma);
    ID3D12Resource               *GetNormalMap();
    ID3D12Resource               *GetAmbientMap();
    CD3DX12_CPU_DESCRIPTOR_HANDLE GetNormalMapRTV() const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetNormalMapSRV() const;
    CD3DX12_GPU_DESCRIPTOR_HANDLE GetAmbientMapSRV() const;
    void                          BuildDescriptors(ID3D12Resource *depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
                                                   CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
                                                   CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
                                                   UINT                          cbvSrvUavDescriptorSize,
                                                   UINT                          rtvDescriptorSize);
    void                          RebuildDescriptors(ID3D12Resource *depthStencilBuffer);
    void                          SetPSOs(ID3D12PipelineState *ssaoPso);
    void                          ComputeSsao(ID3D12GraphicsCommandList *cmdList);
    void                          BuildResources();
    void                          BuildRandomVectorTexture(ID3D12GraphicsCommandList *cmdList);
    void                          BuildOffsetVectors();
    void                          UploadConstants(DirectX::XMMATRIX proj);

    ID3D12Device                 *device = nullptr;
    ID3D12RootSignature          *rootSignature = nullptr;
    ID3D12PipelineState          *mSsaoPso = nullptr;
    ID3D12Resource               *mRandomVectorMap;
    ID3D12Resource               *mRandomVectorMapUploadBuffer;
    ID3D12Resource               *mNormalMap;
    ID3D12Resource               *mAmbientMap0;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhNormalMapGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhNormalMapCpuRtv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhDepthMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhDepthMapGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhRandomVectorMapCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhRandomVectorMapGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mhAmbientMap0GpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mhAmbientMap0CpuRtv;
    UINT                          mRenderTargetWidth;
    UINT                          mRenderTargetHeight;
    DirectX::XMFLOAT4             mOffsets[14];
    D3D12_VIEWPORT                mViewport;
    D3D12_RECT                    mScissorRect;
    ID3D12Resource               *cbSSAOUploadBuffer;
    BYTE                         *cbSSAOMapping = nullptr;
};

#endif //!_DX12SSAO_PASS_H_
