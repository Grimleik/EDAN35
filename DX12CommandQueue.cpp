#include "DX12CommandQueue.h"
#include "Common_DX12.h"
#include <BaseTsd.h>

DX12CommandQueue::DX12CommandQueue(ID3D12Device5 *device, D3D12_COMMAND_LIST_TYPE type) : device(device), commandListType(type), fenceValue(0) {
    D3D12_COMMAND_QUEUE_DESC desc = {};
    desc.Type = type;
    desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    desc.NodeMask = 0;

    DX12_HR(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&commandQueue)), L"Failed to create command queue");
    DX12_HR(device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence)), L"Failed to create a fence.");

    fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    assert(fenceEvent && "Failed to create fence event.");
}

DX12CommandQueue::~DX12CommandQueue() {

    while (!commandListQueue.empty()) {
        auto &ele = commandListQueue.front();
        DX12_RELEASE(ele);
        commandListQueue.pop();
    }

    while (!commandAllocatorQueue.empty()) {
        auto &ele = commandAllocatorQueue.front();
        DX12_RELEASE(ele.commandAllocator);
        commandAllocatorQueue.pop();
    }

    ::CloseHandle(fenceEvent);
    DX12_RELEASE(fence);
    DX12_RELEASE(commandQueue);
}

uint64_t DX12CommandQueue::Signal() {
    uint64_t result = ++fenceValue;
    DX12_HR(commandQueue->Signal(fence, result), L"Failed to signal command queue.");
    return result;
}

bool DX12CommandQueue::IsFenceComplete(uint64_t fenceValue) {
    return fence->GetCompletedValue() >= fenceValue;
}

static constexpr DWORD SOME_MAGIC_WAIT_VALUE = {100000};

void DX12CommandQueue::WaitForFenceValue(uint64_t fenceValue) {
    if (!IsFenceComplete(fenceValue)) {
        fence->SetEventOnCompletion(fenceValue, fenceEvent);
        ::WaitForSingleObject(fenceEvent, SOME_MAGIC_WAIT_VALUE);
    }
}

void DX12CommandQueue::Flush() {
    WaitForFenceValue(Signal());
}

ID3D12GraphicsCommandList2 *DX12CommandQueue::GetCommandList() {

    ID3D12CommandAllocator     *commandAllocator;
    ID3D12GraphicsCommandList2 *result;
    if (!commandAllocatorQueue.empty() && IsFenceComplete(commandAllocatorQueue.front().fenceValue)) {
        commandAllocator = commandAllocatorQueue.front().commandAllocator;

        commandAllocatorQueue.pop();
        DX12_HR(commandAllocator->Reset(), L"Failed to reset command allocator.");
    } else {
        commandAllocator = CreateCommandAllocator();
    }

    if (!commandListQueue.empty()) {
        result = commandListQueue.front();
        commandListQueue.pop();

        DX12_HR(result->Reset(commandAllocator, nullptr), L"Failed to reset command list.");
    } else {
        result = CreateCommandList(commandAllocator);
    }

    DX12_HR(result->SetPrivateDataInterface(__uuidof(ID3D12CommandAllocator), commandAllocator), L"Failed to initialized command allocator");

    return result;
}

ID3D12CommandQueue *DX12CommandQueue::GetCommandQueue() const {
    return commandQueue;
}

uint64_t DX12CommandQueue::ExecuteCommandList(ID3D12GraphicsCommandList2 *commandList) {
    commandList->Close();

    ID3D12CommandAllocator *commandAllocator;
    UINT                    dataSize = sizeof(commandAllocator);

    DX12_HR(commandList->GetPrivateData(__uuidof(ID3D12CommandAllocator), &dataSize, &commandAllocator), L"Failed to fetch data from command allocator during execution.");

    ID3D12CommandList *const ppCommandLists[] = {
        commandList,
    };

    commandQueue->ExecuteCommandLists(1, ppCommandLists);
    uint64_t fenceValue = Signal();

    commandAllocatorQueue.emplace(CommandAllocatorEntry{fenceValue, commandAllocator});
    commandListQueue.push(commandList);

    DX12_RELEASE(commandAllocator);

    return fenceValue;
}

ID3D12CommandAllocator *DX12CommandQueue::CreateCommandAllocator() {
    ID3D12CommandAllocator *commandAllocator;
    DX12_HR(device->CreateCommandAllocator(commandListType, IID_PPV_ARGS(&commandAllocator)), L"Failed to create command allocator.");
    return commandAllocator;
}

ID3D12GraphicsCommandList2 *DX12CommandQueue::CreateCommandList(ID3D12CommandAllocator *allocator) {
    ID3D12GraphicsCommandList2 *commandList;
    DX12_HR(device->CreateCommandList(0, commandListType, allocator, nullptr, IID_PPV_ARGS(&commandList)), L"Failed to create command allocator.");
    return commandList;
}
