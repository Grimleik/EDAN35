#ifndef _DX12_H_
#define _DX12_H_

/* This DX12 Wrapper class is based on the introductory material found in $SOURCE
 * SOURCE: https://www.3dgep.com/learning-directx-12-1
 */

#include "Common.h"
#include "Common_DX12.h"

#include "DX12CommandQueue.h"

static constexpr uint8_t           NUM_FRAMES = {3};
static constexpr D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL::D3D_FEATURE_LEVEL_12_1;

struct DX12 {
    DX12(const HWND &_hwnd, uint32_t w, uint32_t h);
    ~DX12();

    void Initialize();
    void Resize(uint32_t w, uint32_t h);
    void Render(DirectX::XMMATRIX modelMatrix,
                DirectX::XMMATRIX viewMatrix,
                DirectX::XMMATRIX projectionMatrix);
    void CleanUp();

    void TransitionResource(ID3D12GraphicsCommandList2 *commandList,
                            ID3D12Resource             *resource,
                            D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);

    void ClearRTV(ID3D12GraphicsCommandList2 *commandList,
                  D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT *clearColor);

    void ClearDepth(ID3D12GraphicsCommandList2 *commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth = 1.0f);

    void ResizeDepthBuffer(uint32_t w, uint32_t h);

    void UpdateBufferResource(ID3D12GraphicsCommandList2 *commandList, ID3D12Resource **dest, ID3D12Resource **intermediare,
                              size_t numElements, size_t elementSize, const void *bufferData, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    void LoadContent();

    void ToggleFullscreen();

    const HWND              &hwnd;
    RECT                     windowRect;
    uint32_t                 windowWidth, windowHeight;
    ID3D12Device5           *device = {nullptr};
    IDXGISwapChain4         *swapChain = {nullptr};
    ID3D12DescriptorHeap    *rtvDescriptorHeap = {nullptr};
    ID3D12Resource          *backBuffers[NUM_FRAMES];
    unsigned int             rtvDescriptorSize = {0};
    unsigned int             currentBackBufferIndex = {0};
    uint64_t                 frameFenceValues[NUM_FRAMES] = {};
    bool                     vSync = {true};
    bool                     tearingSupported = {false};
    bool                     fullscreen = {false};
    ID3D12DescriptorHeap    *dsvHeap = {nullptr};
    ID3D12Resource          *vertexBuffer = {nullptr};
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    ID3D12Resource          *indexBuffer = {nullptr};
    D3D12_INDEX_BUFFER_VIEW  indexBufferView;
    ID3D12Resource          *depthBuffer = {nullptr};
    ID3D12RootSignature     *rootSignature = {nullptr};
    ID3D12PipelineState     *pipeLineState = {nullptr};
    D3D12_VIEWPORT           viewPort;
    D3D12_RECT               scissorRect;

  private:
    void              UpdateRenderTargetViews();
    void              Flush();
    WINDOWPLACEMENT   wpPrev = {sizeof(WINDOWPLACEMENT)};
    DX12CommandQueue *directCQ;
    DX12CommandQueue *computeCQ;
    DX12CommandQueue *copyCQ;
};

#endif //!_DX12_H_