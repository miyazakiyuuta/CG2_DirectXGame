#include "RenderTarget.h"
#include "base/SrvManager.h"

#include <cassert>

void RenderTarget::Create(ID3D12Device* device, SrvManager* srvManager, D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle, 
    uint32_t width, uint32_t height, DXGI_FORMAT format) {

	width_ = width;
	height_ = height;
    rtvHandle_ = rtvHandle;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resDesc.Width = width;
    resDesc.Height = height;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = format;
    resDesc.SampleDesc.Count = 1;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = format;
    memcpy(clearValue.Color, clearColor_, sizeof(float) * 4);

    HRESULT hr = device->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue,
        IID_PPV_ARGS(&texture_));
    assert(SUCCEEDED(hr));

    device->CreateRenderTargetView(texture_.Get(), nullptr, rtvHandle_);

    srvIndex_ = srvManager->Allocate();
    srvManager->CreateRenderTextureSrv(srvIndex_, texture_.Get(), format);
}

void RenderTarget::BeginRender(ID3D12GraphicsCommandList* commandList, D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle) {

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList->ResourceBarrier(1, &barrier);

    commandList->OMSetRenderTargets(1, &rtvHandle_, false, &dsvHandle);

    D3D12_VIEWPORT viewport = { 0, 0, (float)width_, (float)height_, 0, 1 };
    D3D12_RECT scissor = { 0, 0, (LONG)width_, (LONG)height_ };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);

    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);
    commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
}

void RenderTarget::BeginRenderNoDepth(ID3D12GraphicsCommandList* commandList) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commandList->ResourceBarrier(1, &barrier);

    commandList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);

    D3D12_VIEWPORT viewport = { 0, 0, (float)width_, (float)height_, 0, 1 };
    D3D12_RECT     scissor = { 0, 0, (LONG)width_, (LONG)height_ };
    commandList->RSSetViewports(1, &viewport);
    commandList->RSSetScissorRects(1, &scissor);
    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);

}

void RenderTarget::EndRender(ID3D12GraphicsCommandList* cmdList) {
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = texture_.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}
