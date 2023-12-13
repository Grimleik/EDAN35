#ifndef _DX12_H_
#define _DX12_H_

/* This DX12 Wrapper class is based on the introductory material found in $SOURCE
 * SOURCE: https://www.3dgep.com/learning-directx-12-1
 */

#include "Common_DX12.h"
#include "DX12RenderMesh.h"
#include "DX12SSAOPass.h"

#include "DX12CommandQueue.h"

static constexpr uint8_t           NUM_FRAMES = {3};
static constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;

struct CBConstants {
    DirectX::XMMATRIX World;
    DirectX::XMMATRIX View;
    DirectX::XMMATRIX ViewProj;
};

struct DX12 {
    DX12(const HWND &_hwnd, uint32_t w, uint32_t h);
    ~DX12();

    void Initialize();
    void CreateShadersAndPSOs();
    void UploadConstantBuffer(DirectX::XMMATRIX world, DirectX::XMMATRIX view, DirectX::XMMATRIX viewProj);
    void DrawRenderMesh(ID3D12GraphicsCommandList2 *commandList, DX12RenderMesh rm);
    void UpdateAndRender(DirectX::XMMATRIX modelMatrix,
                         DirectX::XMMATRIX viewMatrix,
                         DirectX::XMMATRIX projectionMatrix);
    void CleanUp();
    void TransitionResource(ID3D12GraphicsCommandList2 *commandList,
                            ID3D12Resource             *resource,
                            D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    void ClearRTV(ID3D12GraphicsCommandList2 *commandList,
                  D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT *clearColor);
    void ClearDepth(ID3D12GraphicsCommandList2 *commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.0f);
    void LoadContent();
    void Flush();

    ID3D12Resource *CreateDefaultBuffer(ID3D12Device *device, ID3D12GraphicsCommandList *cmdList, const void *initData, UINT64 byteSize, ID3D12Resource **uploadBuffer);

    const HWND           &hwnd;
    uint32_t              windowWidth, windowHeight;
    ID3D12Device5        *device = {nullptr};
    IDXGISwapChain4      *swapChain = {nullptr};
    ID3D12DescriptorHeap *rtvDescriptorHeap = {nullptr};
    ID3D12Resource       *backBuffers[NUM_FRAMES];
    unsigned int          rtvDescriptorSize = {0};
    unsigned int          cbvSrvUavDescriptorSize = 0;
    unsigned int          currentBackBufferIndex = {0};
    uint64_t              frameFenceValues[NUM_FRAMES] = {};
    ID3D12DescriptorHeap *dsvHeap = {nullptr};
    ID3D12Resource       *depthBuffer = {nullptr};
    ID3D12RootSignature  *rootSignature = {nullptr};
    D3D12_VIEWPORT        viewPort;
    D3D12_RECT            scissorRect;
    ID3D12RootSignature  *ssaoRootSignature = {nullptr};
    DX12CommandQueue     *directCQ;
    DX12RenderMesh        renderSkull;
    ID3D12DescriptorHeap *srvDescriptorHeap;
    DX12SSAOPass          ssaoPass;
    ID3D12PipelineState  *normalPSO;
    ID3D12PipelineState  *ssaoPSO;
    ID3D12PipelineState  *drawSSAOPSO;
    DXGI_FORMAT           mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT           mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ID3D12Resource       *cbConstantUploadBuffer;
    BYTE                 *cbDataMapping = nullptr;
};

#endif //!_DX12_H_