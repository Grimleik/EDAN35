#include "DX12.h"
#include <array>
#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <fstream>
#include <iostream>

#pragma comment(lib, "windowscodecs.lib")

using namespace DirectX;

DX12::DX12(const HWND &_hwnd, uint32_t w, uint32_t h) : hwnd(_hwnd),
                                                        scissorRect(CD3DX12_RECT(0, 0, (LONG)(w), (LONG)(h))),
                                                        viewPort(CD3DX12_VIEWPORT(0.0f, 0.0f, (FLOAT)(w), (FLOAT)(h))) {
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
    // dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
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
    auto commandList = directCQ->GetCommandList();

    //  .. create the swap chain ..
    IDXGIFactory5 *dxgiFactory4;
    createFactoryFlags = 0;
#if defined(_DEBUG)
    createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

    DX12_HR(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&dxgiFactory4)), L"Failed to create a Factory.");

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = windowWidth;
    swapChainDesc.Height = windowHeight;
    swapChainDesc.Format = mBackBufferFormat;
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
    D3D12_DESCRIPTOR_HEAP_DESC rtv_desc = {};
    rtv_desc.NumDescriptors = NUM_FRAMES + 2; // Normal and SSAO map.
    rtv_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtv_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtv_desc.NodeMask = 0;
    DX12_HR(device->CreateDescriptorHeap(&rtv_desc, IID_PPV_ARGS(&rtvDescriptorHeap)), L"Failed to create descriptor heap.");

    rtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // .. setup our render target views ..
    UpdateRenderTargetViews();

    // .. our depthstencilview ..
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask = 0;
    DX12_HR(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&dsvHeap)), L"Failed to create descriptor heap.");

    D3D12_CLEAR_VALUE optimizedCV = {};
    optimizedCV.Format = mDepthStencilFormat;
    optimizedCV.DepthStencil = {1.0f, 0};

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto initial_state = CD3DX12_RESOURCE_DESC::Tex2D(mDepthStencilFormat, windowWidth, windowHeight,
                                                      1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

    DX12_HR(device->CreateCommittedResource(&heap_prop, D3D12_HEAP_FLAG_NONE, &initial_state,
                                            D3D12_RESOURCE_STATE_DEPTH_WRITE, &optimizedCV, IID_PPV_ARGS(&depthBuffer)),
            L"Failed to commited Depth Buffer.");

    D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
    dsv.Format = mDepthStencilFormat;
    dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsv.Texture2D.MipSlice = 0;
    dsv.Flags = D3D12_DSV_FLAG_NONE;

    device->CreateDepthStencilView(depthBuffer, &dsv, dsvHeap->GetCPUDescriptorHandleForHeapStart());

    heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto init_state = CD3DX12_RESOURCE_DESC::Buffer((sizeof(CBConstants) + 255) & ~255);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &init_state,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&cbConstantUploadBuffer)),
            L"");

    DX12_HR(cbConstantUploadBuffer->Map(0, nullptr, reinterpret_cast<void **>(&cbDataMapping)), L"");

    mRtvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    mDsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    mCbvSrvUavDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    directCQ->ExecuteCommandList(commandList);
    Flush();
}

void DX12::UpdateAndRender(DirectX::XMMATRIX modelMatrix, DirectX::XMMATRIX viewMatrix, DirectX::XMMATRIX projectionMatrix) {
    // UPDATE:

    auto                        commandQueue = directCQ;
    ID3D12GraphicsCommandList2 *commandList = commandQueue->GetCommandList();

    UpdateNormalConstantBuffer(modelMatrix, viewMatrix, projectionMatrix);
    ssaoPass.UploadConstants(projectionMatrix);

    // RENDER:
    auto                          backBuffer = backBuffers[currentBackBufferIndex];
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtv(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), currentBackBufferIndex, rtvDescriptorSize);
    auto                          dsv = dsvHeap->GetCPUDescriptorHandleForHeapStart();

    ID3D12DescriptorHeap *descriptorHeaps[] = {srvDescriptorHeap};
    commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
    commandList->SetGraphicsRootSignature(rootSignature);

    // Draw Normals..
    commandList->RSSetViewports(1, &viewPort);
    commandList->RSSetScissorRects(1, &scissorRect);

    auto normalMap = ssaoPass.GetNormalMap();
    auto normalMapRtv = ssaoPass.GetNormalMapRTV();
    TransitionResource(commandList, normalMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);

    float clearValue[] = {0.0f, 0.0f, 1.0f, 0.0f};
    commandList->ClearRenderTargetView(normalMapRtv, clearValue, 0, nullptr);
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    commandList->OMSetRenderTargets(1, &normalMapRtv, true, &dsv);
    commandList->SetPipelineState(normalPSO);
    DrawRenderMesh(commandList, renderSkull, cbConstantUploadBuffer);

    TransitionResource(commandList, normalMap, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);

    // .. draw SSAO.
    commandList->SetGraphicsRootSignature(ssaoRootSignature);
    ssaoPass.ComputeSsao(commandList);

    commandList->SetGraphicsRootSignature(rootSignature);
    commandList->RSSetViewports(1, &viewPort);
    commandList->RSSetScissorRects(1, &scissorRect);

    TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

    float clearColor[] = {0.4f, 0.6f, 0.9f, 1.0f};
    ClearRTV(commandList, rtv, clearColor);

    commandList->OMSetRenderTargets(1, &rtv, true, &dsv);

    commandList->SetGraphicsRootConstantBufferView(0, cbConstantUploadBuffer->GetGPUVirtualAddress());
    CD3DX12_GPU_DESCRIPTOR_HANDLE ssaoDescriptor(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    ssaoDescriptor.Offset(0, mCbvSrvUavDescriptorSize);
    commandList->SetGraphicsRootDescriptorTable(2, ssaoDescriptor);

    // .. sample ssao onto a fullscreen effect.
    commandList->SetPipelineState(drawSSAOPSO);
    commandList->IASetVertexBuffers(0, 0, nullptr);
    commandList->IASetIndexBuffer(nullptr);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->DrawInstanced(6, 1, 0, 0);

    TransitionResource(commandList, backBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

    frameFenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);
    UINT syncInterval = vSync ? 1 : 0;
    UINT presentFlags = tearingSupported && !vSync ? DXGI_PRESENT_ALLOW_TEARING : 0;

    DX12_HR(swapChain->Present(syncInterval, presentFlags), L"Failed to swap back buffers.");
    currentBackBufferIndex = swapChain->GetCurrentBackBufferIndex();
    commandQueue->WaitForFenceValue(frameFenceValues[currentBackBufferIndex]);
}

void DX12::CleanUp() {

    Flush();

    delete directCQ;
    directCQ = nullptr;

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
}

ID3D12Resource *CreateDefaultBuffer(
    ID3D12Device              *device,
    ID3D12GraphicsCommandList *cmdList,
    const void                *initData,
    UINT64                     byteSize,
    ID3D12Resource           **uploadBuffer) {

    ID3D12Resource *defaultBuffer;

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    auto initial_state = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &initial_state,
                D3D12_RESOURCE_STATE_COMMON,
                nullptr,
                IID_PPV_ARGS(&defaultBuffer)),
            L"");

    heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &initial_state,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(uploadBuffer)),
            L"");

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = byteSize;
    subResourceData.SlicePitch = subResourceData.RowPitch;

    auto barrier1 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &barrier1);
    UpdateSubresources<1>(cmdList, defaultBuffer, *uploadBuffer, 0, 0, 1, &subResourceData);
    auto barrier2 = CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &barrier2);

    return defaultBuffer;
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

    auto commandQueue = directCQ;
    auto commandList = commandQueue->GetCommandList();
    {
        std::ifstream fin("models/skull.txt");

        UINT        vCount = 0;
        UINT        tCount = 0;
        std::string ignore;
        fin >> ignore >> vCount;
        fin >> ignore >> tCount;
        fin >> ignore >> ignore >> ignore >> ignore;

        XMFLOAT3 vMinf3(FLT_MAX, FLT_MAX, FLT_MAX);
        XMFLOAT3 vMaxf3(-FLT_MAX, -FLT_MAX, -FLT_MAX);

        XMVECTOR vMin = XMLoadFloat3(&vMinf3);
        XMVECTOR vMax = XMLoadFloat3(&vMaxf3);

        std::vector<Vertex> vertices(vCount);
        for (UINT i = 0; i < vCount; ++i) {
            fin >> vertices[i].Pos.x >> vertices[i].Pos.y >> vertices[i].Pos.z;
            fin >> vertices[i].Normal.x >> vertices[i].Normal.y >> vertices[i].Normal.z;

            vertices[i].TexC = {0.0f, 0.0f};

            XMVECTOR P = XMLoadFloat3(&vertices[i].Pos);

            XMVECTOR N = XMLoadFloat3(&vertices[i].Normal);

            XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            if (fabsf(XMVectorGetX(XMVector3Dot(N, up))) < 1.0f - 0.001f) {
                XMVECTOR T = XMVector3Normalize(XMVector3Cross(up, N));
                XMStoreFloat3(&vertices[i].TangentU, T);
            } else {
                up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
                XMVECTOR T = XMVector3Normalize(XMVector3Cross(N, up));
                XMStoreFloat3(&vertices[i].TangentU, T);
            }

            vMin = XMVectorMin(vMin, P);
            vMax = XMVectorMax(vMax, P);
        }

        fin >> ignore;
        fin >> ignore;
        fin >> ignore;

        std::vector<std::int32_t> indices(3 * tCount);
        for (UINT i = 0; i < tCount; ++i) {
            fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
        }

        fin.close();

        const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
        const UINT ibByteSize = (UINT)indices.size() * sizeof(std::int32_t);
        DX12_HR(D3DCreateBlob(vbByteSize, &renderSkull.vertexBufferCPU), L"Failed to initialize rendermesh vertex buffer");
        CopyMemory(renderSkull.vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

        DX12_HR(D3DCreateBlob(ibByteSize, &renderSkull.indexBufferCPU), L"");
        CopyMemory(renderSkull.indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

        renderSkull.vertexBufferGPU = CreateDefaultBuffer(device, commandList, vertices.data(), vbByteSize, &renderSkull.vertexBufferUploader);

        renderSkull.indexBufferGPU = CreateDefaultBuffer(device, commandList, indices.data(), ibByteSize, &renderSkull.indexBufferUploader);

        renderSkull.vertexByteStride = sizeof(Vertex);
        renderSkull.vertexBufferByteSize = vbByteSize;
        renderSkull.indexFormat = DXGI_FORMAT_R32_UINT;
        renderSkull.indexBufferByteSize = ibByteSize;
        renderSkull.indexCount = (UINT)indices.size();
        renderSkull.startIndexLoc = 0;
        renderSkull.baseVertexLoc = 0;
    }

    // .. initialize our ssao pass ..
    ssaoPass.Initialize(device, commandList, windowWidth, windowHeight, viewPort, scissorRect);

    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
    CD3DX12_ROOT_PARAMETER rootParameters[3];
    rootParameters[0].InitAsConstantBufferView(0);
    rootParameters[1].InitAsShaderResourceView(0, 1);
    rootParameters[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);

    CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                        D3D12_TEXTURE_ADDRESS_MODE_WRAP);

    CD3DX12_ROOT_SIGNATURE_DESC rootDesc = {};
    rootDesc.NumParameters = _countof(rootParameters);
    rootDesc.pParameters = rootParameters;
    rootDesc.NumStaticSamplers = 1;
    rootDesc.pStaticSamplers = &sampler;
    rootDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

    ID3DBlob *rootSignatureBlob = nullptr;
    ID3DBlob *errorBlob = nullptr;
    DX12_HR(D3D12SerializeRootSignature(&rootDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob), L"");
    DX12_HR(device->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)), L"Failed to create root signature.");

    // SSAO root:
    CreateSSAORootSignature();

    D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
    srvDesc.NumDescriptors = 4;
    srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    DX12_HR(device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&srvDescriptorHeap)), L"");

    // CPU
    auto srvCPU = CD3DX12_CPU_DESCRIPTOR_HANDLE(srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    srvCPU.Offset(0, mCbvSrvUavDescriptorSize);

    // GPU
    auto srvGPU = CD3DX12_GPU_DESCRIPTOR_HANDLE(srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
    srvGPU.Offset(0, mCbvSrvUavDescriptorSize);

    // RTV
    auto rtv = CD3DX12_CPU_DESCRIPTOR_HANDLE(rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
    rtv.Offset(NUM_FRAMES, mRtvDescriptorSize);

    ssaoPass.BuildDescriptors(depthBuffer, srvCPU, srvGPU, rtv, mCbvSrvUavDescriptorSize, mRtvDescriptorSize);

    CreateShadersAndPSOs();
    ssaoPass.SetPSOs(ssaoPSO);

    uint64_t fenceValue = commandQueue->ExecuteCommandList(commandList);
    commandQueue->WaitForFenceValue(fenceValue);

    Flush();

    DX12_RELEASE(rootSignatureBlob);
    DX12_RELEASE(errorBlob);
}

void DX12::UpdateNormalConstantBuffer(DirectX::XMMATRIX world, DirectX::XMMATRIX view, DirectX::XMMATRIX proj) {
    XMMATRIX viewProj = XMMatrixMultiply(view, proj);

    CBConstants constantCB = {};
    constantCB.World = XMMatrixTranspose(world);
    constantCB.View = XMMatrixTranspose(view);
    constantCB.ViewProj = XMMatrixTranspose(viewProj);
    memcpy(cbDataMapping, &constantCB, sizeof(constantCB));
}

void DX12::UpdateSSAOConstants(DirectX::XMMATRIX proj) {
    
}

void DX12::DrawRenderMesh(ID3D12GraphicsCommandList2 *cmdList, DX12RenderMesh rm, ID3D12Resource *mapping) {

    auto vbv = rm.VertexBufferView();
    cmdList->IASetVertexBuffers(0, 1, &vbv);
    auto ibv = rm.IndexBufferView();
    cmdList->IASetIndexBuffer(&ibv);
    cmdList->IASetPrimitiveTopology(rm.primitiveType);

    if (mapping) {
        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = mapping->GetGPUVirtualAddress();
        cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
    }

    cmdList->DrawIndexedInstanced(rm.indexCount, 1, rm.startIndexLoc, rm.baseVertexLoc, 0);
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
}

void DX12::CreateSSAORootSignature() {
    CD3DX12_DESCRIPTOR_RANGE texTable0;
    texTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0, 0);

    CD3DX12_DESCRIPTOR_RANGE texTable1;
    texTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2, 0);

    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    // Perfomance TIP: Order from most frequent to least frequent.
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstants(1, 1);
    slotRootParameter[2].InitAsDescriptorTable(1, &texTable0, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[3].InitAsDescriptorTable(1, &texTable1, D3D12_SHADER_VISIBILITY_PIXEL);

    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
        0,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_POINT,    // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
        1,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

    const CD3DX12_STATIC_SAMPLER_DESC depthMapSam(
        2,                                 // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,   // filter
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
        D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
        0.0f,
        0,
        D3D12_COMPARISON_FUNC_LESS_EQUAL,
        D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE);

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
        3,                                // shaderRegister
        D3D12_FILTER_MIN_MAG_MIP_LINEAR,  // filter
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
        D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
        D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

    std::array<CD3DX12_STATIC_SAMPLER_DESC, 4> staticSamplers =
        {
            pointClamp, linearClamp, depthMapSam, linearWrap};

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4, slotRootParameter,
                                            (UINT)staticSamplers.size(), staticSamplers.data(),
                                            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ID3DBlob *serializedRootSig = nullptr;
    ID3DBlob *errorBlob = nullptr;
    DX12_HR(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                        &serializedRootSig, &errorBlob),
            L"");
    DX12_HR(device->CreateRootSignature(
                0,
                serializedRootSig->GetBufferPointer(),
                serializedRootSig->GetBufferSize(),
                IID_PPV_ARGS(&ssaoRootSignature)),
            L"");
}

void DX12::CreateShadersAndPSOs() {

    // Vertex Shader:
    ID3DBlob *ssaoVSBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/SSAOVS.cso", &ssaoVSBlob), L"Failed to load vertex shader cso");
    ID3DBlob *normalsVSBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/NormalsVS.cso", &normalsVSBlob), L"Failed to load vertex shader cso");
    ID3DBlob *drawSSAOVSBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/DrawSSAOVS.cso", &drawSSAOVSBlob), L"Failed to load vertex shader cso.");
    ID3DBlob *ssaoPSBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/SSAOPS.cso", &ssaoPSBlob), L"Failed to load pixel shader cso.");
    ID3DBlob *normalsPSBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/NormalsPS.cso", &normalsPSBlob), L"Failed to load pixel shader cso.");
    ID3DBlob *drawSSAOPSBlob;
    DX12_HR(D3DReadFileToBlob(L"x64/Debug/DrawSSAOPS.cso", &drawSSAOPSBlob), L"Failed to load pixel shader cso.");

    D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };

    D3D12_GRAPHICS_PIPELINE_STATE_DESC basePsoDesc;

    ZeroMemory(&basePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    basePsoDesc.InputLayout = {inputLayout, (UINT)_countof(inputLayout)};
    basePsoDesc.pRootSignature = rootSignature;
    basePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    basePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    basePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    basePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    basePsoDesc.SampleMask = UINT_MAX;
    basePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    basePsoDesc.NumRenderTargets = 1;
    basePsoDesc.RTVFormats[0] = mBackBufferFormat;
    basePsoDesc.DSVFormat = mDepthStencilFormat;

    //
    // PSO for debug layer.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = basePsoDesc;
    debugPsoDesc.pRootSignature = rootSignature;
    debugPsoDesc.VS = CD3DX12_SHADER_BYTECODE(drawSSAOVSBlob);
    debugPsoDesc.PS = CD3DX12_SHADER_BYTECODE(drawSSAOPSBlob);
    debugPsoDesc.SampleDesc.Count = 1;
    debugPsoDesc.SampleDesc.Quality = 0;
    DX12_HR(device->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&drawSSAOPSO)), L"");

    //
    // PSO for drawing normals.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC drawNormalsPsoDesc = basePsoDesc;
    drawNormalsPsoDesc.VS = CD3DX12_SHADER_BYTECODE(normalsVSBlob);
    drawNormalsPsoDesc.PS = CD3DX12_SHADER_BYTECODE(normalsPSBlob);
    drawNormalsPsoDesc.RTVFormats[0] = DX12SSAOPass::normalMapFormat;
    drawNormalsPsoDesc.SampleDesc.Count = 1;
    drawNormalsPsoDesc.SampleDesc.Quality = 0;
    drawNormalsPsoDesc.DSVFormat = mDepthStencilFormat;
    DX12_HR(device->CreateGraphicsPipelineState(&drawNormalsPsoDesc, IID_PPV_ARGS(&normalPSO)), L"");

    //
    // PSO for SSAO.
    //
    D3D12_GRAPHICS_PIPELINE_STATE_DESC ssaoPsoDesc = basePsoDesc;
    ssaoPsoDesc.InputLayout = {nullptr, 0};
    ssaoPsoDesc.pRootSignature = ssaoRootSignature;
    ssaoPsoDesc.VS = CD3DX12_SHADER_BYTECODE(ssaoVSBlob);
    ssaoPsoDesc.PS = CD3DX12_SHADER_BYTECODE(ssaoPSBlob);

    // SSAO effect does not need the depth buffer.
    ssaoPsoDesc.DepthStencilState.DepthEnable = false;
    ssaoPsoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
    ssaoPsoDesc.RTVFormats[0] = DX12SSAOPass::ambientMapFormat;
    ssaoPsoDesc.SampleDesc.Count = 1;
    ssaoPsoDesc.SampleDesc.Quality = 0;
    ssaoPsoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
    DX12_HR(device->CreateGraphicsPipelineState(&ssaoPsoDesc, IID_PPV_ARGS(&ssaoPSO)), L"");

    DX12_RELEASE(ssaoVSBlob);
    DX12_RELEASE(drawSSAOVSBlob);
    DX12_RELEASE(normalsVSBlob);

    DX12_RELEASE(ssaoPSBlob);
    DX12_RELEASE(drawSSAOPSBlob);
    DX12_RELEASE(normalsPSBlob);
}