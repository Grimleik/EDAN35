#ifndef _COMMON_DX_12_H_
#define _COMMON_DX_12_H_

#include <DirectXMath.h>
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

#endif //!_COMMON_DX_12_H_