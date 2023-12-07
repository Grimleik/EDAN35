#ifndef _DX12_COMMAND_QUEUE_H_
#define _DX12_COMMAND_QUEUE_H_

#include <d3d12.h>
#include <queue>

class DX12CommandQueue {
  public:
    DX12CommandQueue(ID3D12Device5 *device, D3D12_COMMAND_LIST_TYPE type);
    ~DX12CommandQueue();

    uint64_t Signal();
    bool     IsFenceComplete(uint64_t fenceValue);
    void     WaitForFenceValue(uint64_t fenceValue);
    void     Flush();

    ID3D12GraphicsCommandList2 *GetCommandList();
    ID3D12CommandQueue         *GetCommandQueue() const;
    uint64_t                    ExecuteCommandList(ID3D12GraphicsCommandList2 *commandList);

  private:
    struct CommandAllocatorEntry {
        uint64_t                fenceValue;
        ID3D12CommandAllocator *commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue<ID3D12GraphicsCommandList2 *>;
    ID3D12CommandAllocator     *CreateCommandAllocator();
    ID3D12GraphicsCommandList2 *CreateCommandList(ID3D12CommandAllocator *allocator);

    D3D12_COMMAND_LIST_TYPE commandListType;
    ID3D12Device5          *device;
    ID3D12CommandQueue     *commandQueue;
    ID3D12Fence            *fence;
    HANDLE                  fenceEvent;
    uint64_t                fenceValue;

    CommandAllocatorQueue commandAllocatorQueue;
    CommandListQueue      commandListQueue;
};

#endif //!_DX12_COMMAND_QUEUE_H_
