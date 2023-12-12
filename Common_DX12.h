#ifndef _COMMON_DX_12_H_
#define _COMMON_DX_12_H_

#include "Common.h"
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <dxgi1_6.h>

#define DX12_HR(a, msg)                       \
    if (FAILED((a))) {                        \
        MessageBoxW(0, msg, L"Error", MB_OK); \
    }
#define DX12_RELEASE(a) \
    if (a) {            \
        a->Release();   \
        a = nullptr;    \
    }

struct Vertex {
    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
    DirectX::XMFLOAT3 TangentU;
    Vertex() {}
    Vertex(
        float px, float py, float pz,
        float nx, float ny, float nz,
        float tx, float ty, float tz,
        float u, float v) : Pos(px, py, pz),
                            Normal(nx, ny, nz),
                            TangentU(tx, ty, tz),
                            TexC(u, v) {}
};

#endif //!_COMMON_DX_12_H_