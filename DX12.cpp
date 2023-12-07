#include "DX12.h"
#include <d3dcompiler.h>
#include <dxgidebug.h>

#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;

struct VertexPosColor {
    XMFLOAT3 position;
    XMFLOAT3 color;
};

static VertexPosColor gVertices[8] = {
    {XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f)}, // 0
    {XMFLOAT3(-1.0f, 1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f)},  // 1
    {XMFLOAT3(1.0f, 1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f)},   // 2
    {XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f)},  // 3
    {XMFLOAT3(-1.0f, -1.0f, 1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f)},  // 4
    {XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f)},   // 5
    {XMFLOAT3(1.0f, 1.0f, 1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f)},    // 6
    {XMFLOAT3(1.0f, -1.0f, 1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f)}    // 7
};

static WORD gIndicies[36] = {
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7};

DX12::DX12(const HWND &_hwnd, uint32_t w, uint32_t h) : hwnd(_hwnd),
                                                        scissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX)),
                                                        viewPort(CD3DX12_VIEWPORT(0.0f, 0.0f, (float)(w), (float)(h))) {
    windowWidth = w;
    windowHeight = h;
}

DX12::~DX12() {
}

// NOTE(pf): Debug function to be used in platform layer atexit
void ReportLiveObjects() {
    IDXGIDebug1 *dxgiDebug;
    DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug));

    dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    //dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
    DX12_RELEASE(dxgiDebug);
}

void DX12::Initialize() {

    // NOTE(pf): Check if we should enable the debug layer..

#if defined(_DEBUG)
    ID3D12Debug *debugInterface;
    DX12_HR(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)), L"Unable to retrieve debug interface.");
    debugInterface->EnableDebugLayer();
    DX12_RELEASE(debugInterface);
    atexit(&ReportLiveObjects);
#endif

    // .. check for tearing support..
    BOOL           allowTearing = FALSE;
    IDXGIFactory4 *factory4;
    if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory4)))) {
        IDXGIFactory5 *factory5;
        if (SUCCEEDED(factory4->QueryInterface(__uuidof(IDXGIFactory5), (LPVOID *)&factory5))) {
            if (FAILED(factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing)))) {
                allowTearing = false;
            }
            DX12_RELEASE(factory5);
        }
    }
    DX12_RELEASE(factory4);
    tearingSupported = allowTearing;

    // .. try to find a suitable adapter on the computer..
    IDXGIFactory6 *dxgiFactory;
    UINT           createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
    DX12_HR(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory)), L"Unable to create a DXGIFactory");

    IDXGIAdapter1 *adapter = nullptr;
    IDXGIAdapter1 *bestAdapter = nullptr;
    SIZE_T         maxDedictedVideoMemory = 0;
    int            bestAdapterIndex = 0;
    for (UINT i = 0; dxgiFactory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 dxgiAdapterDesc1;
        adapter->GetDesc1(&dxgiAdapterDesc1);

        // NOTE(pf): We are checking if the adapter is compatible for our purposes..
        if ((dxgiAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0 &&
            SUCCEEDED(D3D12CreateDevice(adapter, MINIMUM_FEATURE_LEVEL, __uuidof(ID3D12Device), nullptr)) &&
            dxgiAdapterDesc1.DedicatedVideoMemory > maxDedictedVideoMemory) {
            if (bestAdapter) {
                DX12_RELEASE(bestAdapter);
            }
            bestAdapter = adapter;
            maxDedictedVideoMemory = dxgiAdapterDesc1.DedicatedVideoMemory;
        } else {
            DX12_RELEASE(adapter);
        }
    }

    DX12_RELEASE(dxgiFactory);
    if (maxDedictedVideoMemory == 0) {
        MessageBox(0, L"Failed to find a suitable GFX Card.", L"Error", MB_OK);
        return;
    }

    IDXGIAdapter4 *adapter4;
    bestAdapter->QueryInterface(__uuidof(IDXGIAdapter4), (LPVOID *)&adapter4);
    // .. create the device..
    DX12_HR(D3D12CreateDevice(adapter4, MINIMUM_FEATURE_LEVEL, IID_PPV_ARGS(&device)), L"Failed to create a DX12 Device");
    DX12_RELEASE(adapter4);
    DX12_RELEASE(bestAdapter);

#if defined(_DEBUG)
    ID3D12InfoQueue *pInfoQueue;
    if (SUCCEEDED(device->QueryInterface(__uuidof(ID3D12InfoQueue), (LPVOID *)&pInfoQueue))) {
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
        pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

        // D3D12_MESSAGE_CATEGORY Categories[] = {};

        // NOTE(pf): If we want to suppress certain warning flags use this:
        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO};

        D3D12_MESSAGE_ID denyIDs[] = {
            D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
            D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
            D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,
        };

        D3D12_INFO_QUEUE_FILTER newFilter = {};
        // newFilter.DenyList.NumCategories = _countof(Categories);
        // newFilter.DenyList.pCategoryList = Categories;
        newFilter.DenyList.NumSeverities = _countof(severities);
        newFilter.DenyList.pSeverityList = severities;
        newFilter.DenyList.NumIDs = _countof(denyIDs);
        newFilter.DenyList.pIDList = denyIDs;

        DX12_HR(pInfoQueue->PushStorageFilter(&newFilter), L"Failed to push filter list to debug layer.");
        DX12_RELEASE(pInfoQueue);
    }
#endif
    directCQ = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_DIRECT);
    computeCQ = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_COMPUTE);
    copyCQ = new DX12CommandQueue(device, D3D12_COMMAND_LIST_TYPE_COPY);
    // .. create the swap chain ..
    IDXGIFactory5 *dxgiFactory4;
    createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    DX12_HR(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)), L"Failed to create a Factory.");

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = windowWidth;
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.Stereo = FALSE;
    swapChainDesc.SampleDesc = {1, 0};
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = NUM_FRAMES;
    swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swapChainDesc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

    IDXGISwapChain1 *tmpSwapChain;
    DX12_HR(dxgiFactory4->CreateSwapChainForHwnd(directCQ->GetCommandQueue(), hwnd, &swapChainDesc, nullptr, nullptr, &tmpSwapChain), L"Failed to create the swapchain.");
    DX12_HR(dxgiFactory4->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER), L"Failed to disable alt+enter fullscreen.");
    DX12_HR(tmpSwapChain->QueryInterface(__uuidof(IDXGISwapChain4), (LPVOID *)&swapChain), L"Failed to init swapchain.");

    DX12_RELEASE(dxgiFactory4);
    DX12_RELEASE(tmpSwapChain);

    currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

    // .. create the descriptor heap ..
    D3D12_DESCRIPTOR_HEAP_DESC dh_desc = {};
    dh_desc.NumDescriptors = NUM_FRAMES;
    dh_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    DX12_HR(device->CreateDescriptorHeap(&dh_desc, IID_PPV_ARGS(&rtvDescriptorHeap)), L"Failed to create descriptor heap.");

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // .. setup our render target views ..
    UpdateRenderTargetViews();
}

void DX12::Render(DirectX::XMMATRIX modelMatrix,
                  DirectX::XMMATRIX viewMatrix,
                  DirectX::XMMATRIX projectionMatrix) {
    auto commandQueue = directCQ;
    auto commandList = commandQueue->GetCommandList();

    auto                          backBuffer = backBuffers[currentBackBufferIndex];
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rtvDescriptorSize);
    auto                          dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();

    // Clear RTV:
    {
        TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        float clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
        ClearRTV(commandList, rtv, clearColor);
        ClearDepth(commandList, dsv);
    }

    commandList->SetPipelineState(pipeLineState);
    commandList->SetGraphicsRootSignature(rootSignature);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    commandList->IASetIndexBuffer(&indexBufferView);

    commandList->RSSetViewports(1, &viewPort);
    commandList->RSSetScissorRects(1, &scissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);
    XMMATRIX mvpMat = XMMatrixMultiply(modelMatrix, viewMatrix);
    mvpMat = XMMatrixMultiply(mvpMat, projectionMatrix);
    commandList->SetGraphicsRoot32BitConstants(0, sizeof(XMMATRIX) / 4, &mvpMat, 0);

    commandList->DrawIndexedInstanced(_countof(gIndicies), 1, 0, 0, 0);

    // Present:
    {
        TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        frameFenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);
        UINT syncInterval = vSync ? 1 : 0;
        UINT presentFlags = tearingSupported && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;

        DX12_HR(swapChain->Present(syncInterval, presentFlags), L"Failed to swap back buffers.");
        currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
        commandQueue->WaitForFenceValue(frameFenceValues[currentBackBufferIndex]);
    }
}

void DX12::CleanUp() {
    
    Flush();
    
    delete directCQ;
    directCQ = nullptr;
    delete computeCQ;
    computeCQ = nullptr;
    delete copyCQ;
    copyCQ = nullptr;
    

    DX12_RELEASE(pipeLineState);
    DX12_RELEASE(rootSignature);
    DX12_RELEASE(dsvHeap);
    DX12_RELEASE(depthBuffer);
    DX12_RELEASE(indexBuffer);
    DX12_RELEASE(vertexBuffer);
    for (int i = 0; i < NUM_FRAMES; ++i) {
        DX12_RELEASE(backBuffers[i]);
    }

    DX12_RELEASE(rtvDescriptorHeap);
    DX12_RELEASE(swapChain);
    DX12_RELEASE(device);
}

void DX12::TransitionResource(ID3D12GraphicsCommandList2 *commandList, ID3D12Resource *resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after) {
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(resource, before, after);
    commandList->ResourceBarrier(1, &barrier);
}

void DX12::ClearRTV(ID3D12GraphicsCommandList2 *commandList, D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT *clearColor) {
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void DX12::ClearDepth(ID3D12GraphicsCommandList2 *commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsv, float depth) {
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

void DX12::ResizeDepthBuffer(uint32_t w, uint32_t h) {
    Flush();

    w = max(1, w);
    h = max(1, h);

    D3D12_CLEAR_VALUE optimizedCV = {};
    optimizedCV.Format = DXGI_FORMAT_D32_FLOAT;
    optimizedCV.DepthStencil = {1.0f, 0};

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto initial_state = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, w, h,
                                                      1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    DX12_HR(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &initial_state,
                                            D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedCV, IID_PPV_ARGS(&depthBuffer)),
            L"Failed to commited Depth Buffer.");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = DXGI_FORMAT_D32_FLOAT;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Texture2D.MipSlice = 0;
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(depthBuffer, &dsv, dsvHeap->GetCPUDescriptorHandleForHeapStart());
}

void DX12::UpdateBufferResource(ID3D12GraphicsCommandList2 *commandList, ID3D12Resource **dest, ID3D12Resource **intermediare,
                                size_t numElements, size_t elementSize, const void *bufferData, D3D12_RESOURCE_FLAGS flags) {
    size_t bufferSize = numElements * elementSize;

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto initial_state = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);
    DX12_HR(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE,
                                            &initial_state, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(dest)),
            L"Failed to commit dest resource.");

    if (bufferData) {
        auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto initial_state = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
        DX12_HR(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &initial_state,
                                                D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(intermediare)),
                L"Failed to commit intermediare resource.");

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList, *dest, *intermediare, 0, 0, 1, &subresourceData);
    }
}

void DX12::LoadContent() {

    auto commandQueue = copyCQ;
    auto commandList = commandQueue->GetCommandList();

    ID3D12Resource *intermediareVertexBuffer;
    UpdateBufferResource(commandList, &vertexBuffer, &intermediareVertexBuffer, _countof(gVertices), sizeof(VertexPosColor), gVertices);

    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = sizeof(gVertices);
    vertexBufferView.StrideInBytes = sizeof(VertexPosColor);
    
    ID3D12Resource *intermediareIndexBuffer;
    UpdateBufferResource(commandList, &indexBuffer, &intermediareIndexBuffer, _countof(gIndicies), sizeof(WORD), gIndicies);

    indexBufferView.BufferLocation = indexBuffer->GetGPUVirtualAddress();
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    indexBufferView.SizeInBytes = sizeof(gIndicies);

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    DX12_HR(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)), L"Failed to create descriptor heap.");

    // Vertex Shader:
    ID3DBlob *vertexShaderBlob; // cso -> compiled shader object if included in sln.d3dcompiler.lib
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/VertexShader.cso", &vertexShaderBlob), L"Failed to load vertex shader cso");

    ID3DBlob *pixelShaderBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/PixelShader.cso", &pixelShaderBlob), L"Failed to load pixel shader cso.");

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
    featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
    if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        // TODO(pf): Logging.
    }

    D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

    CD3DX12_ROOT_PARAMETER1 rootParameters[1];
    rootParameters[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
    rootSignatureDescription.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

    ID3DBlob *rootSignatureBlob;
    ID3DBlob *errorBlob;
    DX12_HR(D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSignatureBlob, &errorBlob), L"Failed to serialize root signature.");
    DX12_HR(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)), L"Failed to create root signature.");

    struct PipelineStateStream {
        CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE        rootSignature;
        CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT          inputLayout;
        CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY    primitiveTopologyType;
        CD3DX12_PIPELINE_STATE_STREAM_VS                    vs;
        CD3DX12_PIPELINE_STATE_STREAM_PS                    ps;
        CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT  dsvFormat;
        CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS rtvFormats;
    } pss;

    D3D12_RT_FORMAT_ARRAY rtvFormats = {};
    rtvFormats.NumRenderTargets = 1;
    rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

    pss.rootSignature = rootSignature;
    pss.inputLayout = {inputLayout, _countof(inputLayout)};
    pss.primitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pss.vs = CD3DX12_SHADER_BYTECODE(vertexShaderBlob);
    pss.ps = CD3DX12_SHADER_BYTECODE(pixelShaderBlob);
    pss.dsvFormat = DXGI_FORMAT_D32_FLOAT;
    pss.rtvFormats = rtvFormats;

    D3D12_PIPELINE_STATE_STREAM_DESC pssDesc = {
        sizeof(PipelineStateStream), &pss};
    DX12_HR(device->CreatePipelineState(&pssDesc, IID_PPV_ARGS(&pipeLineState)), L"Failed to create pipeline state.");

    uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    ResizeDepthBuffer(windowWidth, windowHeight);

    DX12_RELEASE(vertexShaderBlob);
    DX12_RELEASE(pixelShaderBlob);
    DX12_RELEASE(rootSignatureBlob);
    DX12_RELEASE(errorBlob);
    DX12_RELEASE(intermediareVertexBuffer);
    DX12_RELEASE(intermediareIndexBuffer);
}

void DX12::ToggleFullscreen() {
    DWORD style = GetWindowLong(hwnd, GWL_STYLE);

    if (style & WS_OVERLAPPEDWINDOW) {
        MONITORINFO mi = {sizeof(mi)};
        if (GetWindowPlacement(hwnd, &wpPrev) &&
            GetMonitorInfo(MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY), &mi)) {
            SetWindowLong(hwnd, GWL_STYLE,
                          style & ~WS_OVERLAPPEDWINDOW);
            SetWindowPos(hwnd, HWND_TOP,
                         mi.rcMonitor.left, mi.rcMonitor.top,
                         mi.rcMonitor.right - mi.rcMonitor.left,
                         mi.rcMonitor.bottom - mi.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLong(hwnd, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
        SetWindowPlacement(hwnd, &wpPrev);
        SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
}

void DX12::Resize(uint32_t width, uint32_t height) {
    if (windowWidth != width || windowHeight != height) {
        windowWidth = max(0, width);
        windowHeight = max(0, height);

        Flush();

        for (int i = 0; i < NUM_FRAMES; ++i) {
            backBuffers[i]->Release();
            frameFenceValues[i] = frameFenceValues[currentBackBufferIndex];
        }

        DXGI_SWAP_CHAIN_DESC sc_desc = {};
        DX12_HR(swapChain->GetDesc(&sc_desc), L"Failed to setup swapchain in Resize.");
        DX12_HR(swapChain->ResizeBuffers(NUM_FRAMES, windowWidth, windowHeight, sc_desc.BufferDesc.Format, sc_desc.Flags), L"Failed to resize swapchain buffers.");
        currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();

        UpdateRenderTargetViews();

        viewPort = CD3DX12_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);
        ResizeDepthBuffer(width, height);
    }
}

void DX12::UpdateRenderTargetViews() {
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    for (int i = 0; i < NUM_FRAMES; ++i) {
        ID3D12Resource *backBuffer;
        DX12_HR(swapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer)), L"Failed to retrieve backbuffer from swap chain.");

        device->CreateRenderTargetView(backBuffer, nullptr, rtvHandle);
        backBuffers[i] = backBuffer;
        rtvHandle.Offset(rtvDescriptorSize);
    }
}

void DX12::Flush() {
    directCQ->Flush();
    computeCQ->Flush();
    copyCQ->Flush();
}
