#include "DX12SSAOPass.h"

using namespace DirectX;
using namespace DirectX::PackedVector;

// Returns random float in [0, 1).
static float RandF() {
    return (float)(rand()) / (float)RAND_MAX;
}
// Returns random float in [a, b).
static float RandF(float a, float b) {
    return a + RandF() * (b - a);
}

DX12SSAOPass::DX12SSAOPass() {
}

DX12SSAOPass::~DX12SSAOPass() {
}

void DX12SSAOPass::Initialize(ID3D12Device *_device, ID3D12GraphicsCommandList2 *cmdList, UINT width, UINT height, D3D12_VIEWPORT viewPort, D3D12_RECT scissorRect) {
    device = _device;
    mRenderTargetWidth = width;
    mRenderTargetHeight = height;
    mViewport = viewPort;
    mScissorRect = scissorRect;

    BuildResources();
    BuildOffsetVectors();
    BuildRandomVectorTexture(cmdList);

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto init_state = CD3DX12_RESOURCE_DESC::Buffer((sizeof(SsaoConstants) + 255) & ~255);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &init_state,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&cbSSAOUploadBuffer)),
            L"");

    DX12_HR(cbSSAOUploadBuffer->Map(0, nullptr, reinterpret_cast<void **>(&cbSSAOMapping)), L"");
}

UINT DX12SSAOPass::GetSSAOMapWidth() const {
    return mRenderTargetWidth;
}

UINT DX12SSAOPass::GetSSAOMapHeight() const {
    return mRenderTargetHeight;
}

void DX12SSAOPass::GetOffsetVectors(DirectX::XMFLOAT4 offsets[14]) {
    std::copy(&mOffsets[0], &mOffsets[14], &offsets[0]);
}

std::vector<float> DX12SSAOPass::CalcGaussWeights(float sigma) {
    float twoSigma2 = 2.0f * sigma * sigma;

    // Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
    // For example, for sigma = 3, the width of the bell curve is
    int blurRadius = (int)ceil(2.0f * sigma);

    assert(blurRadius <= maxBlurRadius);

    std::vector<float> weights;
    weights.resize(2 * blurRadius + 1);

    float weightSum = 0.0f;

    for (int i = -blurRadius; i <= blurRadius; ++i) {
        float x = (float)i;

        weights[i + blurRadius] = expf(-x * x / twoSigma2);

        weightSum += weights[i + blurRadius];
    }

    // Divide by the sum so all the weights add up to 1.0.
    for (int i = 0; i < weights.size(); ++i) {
        weights[i] /= weightSum;
    }

    return weights;
}

ID3D12Resource *DX12SSAOPass::GetNormalMap() {
    return mNormalMap;
}

ID3D12Resource *DX12SSAOPass::GetAmbientMap() {
    return mAmbientMap0;
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DX12SSAOPass::GetNormalMapRTV() const {
    return mhNormalMapCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12SSAOPass::GetNormalMapSRV() const {
    return mhNormalMapGpuSrv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DX12SSAOPass::GetAmbientMapSRV() const {
    return mhAmbientMap0GpuSrv;
}

void DX12SSAOPass::BuildDescriptors(ID3D12Resource *depthStencilBuffer, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv, CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv, UINT cbvSrvUavDescriptorSize, UINT rtvDescriptorSize) {
    // Save references to the descriptors.  The Ssao reserves heap space
    // for 5 contiguous Srvs.

    mhAmbientMap0CpuSrv = hCpuSrv;
    mhNormalMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhDepthMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhRandomVectorMapCpuSrv = hCpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mhAmbientMap0GpuSrv = hGpuSrv;
    mhNormalMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhDepthMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);
    mhRandomVectorMapGpuSrv = hGpuSrv.Offset(1, cbvSrvUavDescriptorSize);

    mhNormalMapCpuRtv = hCpuRtv;
    mhAmbientMap0CpuRtv = hCpuRtv.Offset(1, rtvDescriptorSize);

    //  Create the descriptors
    RebuildDescriptors(depthStencilBuffer);
}

void DX12SSAOPass::RebuildDescriptors(ID3D12Resource *depthStencilBuffer) {
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Format = normalMapFormat;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(mNormalMap, &srvDesc, mhNormalMapCpuSrv);

    srvDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    // srvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    device->CreateShaderResourceView(depthStencilBuffer, &srvDesc, mhDepthMapCpuSrv);

    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    device->CreateShaderResourceView(mRandomVectorMap, &srvDesc, mhRandomVectorMapCpuSrv);

    srvDesc.Format = ambientMapFormat;
    device->CreateShaderResourceView(mAmbientMap0, &srvDesc, mhAmbientMap0CpuSrv);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    rtvDesc.Format = normalMapFormat;
    rtvDesc.Texture2D.MipSlice = 0;
    rtvDesc.Texture2D.PlaneSlice = 0;
    device->CreateRenderTargetView(mNormalMap, &rtvDesc, mhNormalMapCpuRtv);

    rtvDesc.Format = ambientMapFormat;
    device->CreateRenderTargetView(mAmbientMap0, &rtvDesc, mhAmbientMap0CpuRtv);
}

void DX12SSAOPass::SetPSOs(ID3D12PipelineState *ssaoPso) {
    mSsaoPso = ssaoPso;
}

void DX12SSAOPass::ComputeSsao(ID3D12GraphicsCommandList *cmdList) {
    cmdList->RSSetViewports(1, &mViewport);
    cmdList->RSSetScissorRects(1, &mScissorRect);

    // We compute the initial SSAO to AmbientMap0.

    // Change to RENDER_TARGET.
    auto pass0_transition0 = CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0,
                                                                  D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET);
    cmdList->ResourceBarrier(1, &pass0_transition0);

    float clearValue[] = {1.0f, 1.0f, 1.0f, 1.0f};
    cmdList->ClearRenderTargetView(mhAmbientMap0CpuRtv, clearValue, 0, nullptr);

    // Specify the buffers we are going to render to.
    cmdList->OMSetRenderTargets(1, &mhAmbientMap0CpuRtv, true, nullptr);

    // Bind the constant buffer for this pass.
    auto ssaoCBAddress = cbSSAOUploadBuffer->GetGPUVirtualAddress();
    cmdList->SetGraphicsRootConstantBufferView(0, ssaoCBAddress);
    cmdList->SetGraphicsRoot32BitConstant(1, 0, 0);

    // Bind the normal and depth maps.
    cmdList->SetGraphicsRootDescriptorTable(2, mhNormalMapGpuSrv);

    // Bind the random vector map.
    cmdList->SetGraphicsRootDescriptorTable(3, mhRandomVectorMapGpuSrv);

    cmdList->SetPipelineState(mSsaoPso);

    // Draw fullscreen quad.
    cmdList->IASetVertexBuffers(0, 0, nullptr);
    cmdList->IASetIndexBuffer(nullptr);
    cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmdList->DrawInstanced(6, 1, 0, 0);

    // Change back to GENERIC_READ so we can read the texture in a shader.
    auto pass1_transition0 = CD3DX12_RESOURCE_BARRIER::Transition(mAmbientMap0,
                                                                  D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &pass1_transition0);
}

void DX12SSAOPass::BuildResources() {
    // Free the old resources if they exist.
    mNormalMap = nullptr;
    mAmbientMap0 = nullptr;

    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mRenderTargetWidth;
    texDesc.Height = mRenderTargetHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = normalMapFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    float               normalClearColor[] = {0.0f, 0.0f, 1.0f, 0.0f};
    CD3DX12_CLEAR_VALUE optClear(normalMapFormat, normalClearColor);
    auto                heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                &optClear,
                IID_PPV_ARGS(&mNormalMap)),
            L"");
    texDesc.Width = mRenderTargetWidth;
    texDesc.Height = mRenderTargetHeight;
    texDesc.Format = ambientMapFormat;

    float ambientClearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    optClear = CD3DX12_CLEAR_VALUE(ambientMapFormat, ambientClearColor);

    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                &optClear,
                IID_PPV_ARGS(&mAmbientMap0)),
            L"");
}

void DX12SSAOPass::BuildRandomVectorTexture(ID3D12GraphicsCommandList *cmdList) {
    D3D12_RESOURCE_DESC texDesc;
    ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = 256;
    texDesc.Height = 256;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop,
                D3D12_HEAP_FLAG_NONE,
                &texDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&mRandomVectorMap)),
            L"");

    //
    // In order to copy CPU memory data into our default buffer, we need to create
    // an intermediate upload heap.
    //

    const UINT   num2DSubresources = texDesc.DepthOrArraySize * texDesc.MipLevels;
    const UINT64 uploadBufferSize = GetRequiredIntermediateSize(mRandomVectorMap, 0, num2DSubresources);

    auto heap_prop_upload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto inital_state_upload = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);
    DX12_HR(device->CreateCommittedResource(
                &heap_prop_upload,
                D3D12_HEAP_FLAG_NONE,
                &inital_state_upload,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(&mRandomVectorMapUploadBuffer)),
            L"");

    XMCOLOR initData[256 * 256];
    for (int i = 0; i < 256; ++i) {
        for (int j = 0; j < 256; ++j) {
            // Random vector in [0,1].  We will decompress in shader to [-1,1].
            XMFLOAT3 v(RandF(), RandF(), RandF());

            initData[i * 256 + j] = XMCOLOR(v.x, v.y, v.z, 0.0f);
        }
    }

    D3D12_SUBRESOURCE_DATA subResourceData = {};
    subResourceData.pData = initData;
    subResourceData.RowPitch = 256 * sizeof(XMCOLOR);
    subResourceData.SlicePitch = subResourceData.RowPitch * 256;

    //
    // Schedule to copy the data to the default resource, and change states.
    // Note that mCurrSol is put in the GENERIC_READ state so it can be
    // read by a shader.
    //

    auto transition0 = CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->ResourceBarrier(1, &transition0);
    UpdateSubresources(cmdList, mRandomVectorMap, mRandomVectorMapUploadBuffer, 0, 0, num2DSubresources, &subResourceData);
    auto transition1 = CD3DX12_RESOURCE_BARRIER::Transition(mRandomVectorMap, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ);
    cmdList->ResourceBarrier(1, &transition1);
}

void DX12SSAOPass::BuildOffsetVectors() {
    // Start with 14 uniformly distributed vectors.  We choose the 8 corners of the cube
    // and the 6 center points along each cube face.  We always alternate the points on
    // opposites sides of the cubes.  This way we still get the vectors spread out even
    // if we choose to use less than 14 samples.

    // 8 cube corners
    mOffsets[0] = XMFLOAT4(+1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[1] = XMFLOAT4(-1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[2] = XMFLOAT4(-1.0f, +1.0f, +1.0f, 0.0f);
    mOffsets[3] = XMFLOAT4(+1.0f, -1.0f, -1.0f, 0.0f);

    mOffsets[4] = XMFLOAT4(+1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[5] = XMFLOAT4(-1.0f, -1.0f, +1.0f, 0.0f);

    mOffsets[6] = XMFLOAT4(-1.0f, +1.0f, -1.0f, 0.0f);
    mOffsets[7] = XMFLOAT4(+1.0f, -1.0f, +1.0f, 0.0f);

    // 6 centers of cube faces
    mOffsets[8] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 0.0f);
    mOffsets[9] = XMFLOAT4(+1.0f, 0.0f, 0.0f, 0.0f);

    mOffsets[10] = XMFLOAT4(0.0f, -1.0f, 0.0f, 0.0f);
    mOffsets[11] = XMFLOAT4(0.0f, +1.0f, 0.0f, 0.0f);

    mOffsets[12] = XMFLOAT4(0.0f, 0.0f, -1.0f, 0.0f);
    mOffsets[13] = XMFLOAT4(0.0f, 0.0f, +1.0f, 0.0f);

    for (int i = 0; i < 14; ++i) {
        // Create random lengths in [0.25, 1.0].
        float s = RandF(0.25f, 1.0f);

        XMVECTOR v = s * XMVector4Normalize(XMLoadFloat4(&mOffsets[i]));

        XMStoreFloat4(&mOffsets[i], v);
    }
}

void DX12SSAOPass::UploadConstants(XMMATRIX proj) {
    SsaoConstants ssaoCB;

    XMMATRIX P = proj;

    // Transform NDC space [-1,+1]^2 to texture space [0,1]^2
    XMMATRIX T(
        0.5f, 0.0f, 0.0f, 0.0f,
        0.0f, -0.5f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.5f, 0.5f, 0.0f, 1.0f);

    ssaoCB.Proj = P;
    auto tt = XMMatrixDeterminant(proj);
    ssaoCB.InvProj = XMMatrixInverse(&tt, proj);
    XMStoreFloat4x4(&ssaoCB.ProjTex, XMMatrixTranspose(P * T));

    GetOffsetVectors(ssaoCB.OffsetVectors);

    auto blurWeights = CalcGaussWeights(2.5f);
    ssaoCB.BlurWeights[0] = XMFLOAT4(&blurWeights[0]);
    ssaoCB.BlurWeights[1] = XMFLOAT4(&blurWeights[4]);
    ssaoCB.BlurWeights[2] = XMFLOAT4(&blurWeights[8]);

    ssaoCB.InvRenderTargetSize = XMFLOAT2(1.0f / GetSSAOMapWidth(), 1.0f / GetSSAOMapHeight());

    // Coordinates given in view space.
    ssaoCB.OcclusionRadius = 0.5f;
    ssaoCB.OcclusionFadeStart = 0.2f;
    ssaoCB.OcclusionFadeEnd = 1.0f;
    ssaoCB.SurfaceEpsilon = 0.05f;

    memcpy(cbSSAOMapping, &ssaoCB, sizeof(ssaoCB));
}
