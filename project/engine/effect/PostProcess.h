#pragma once
#include "base/DirectXCommon.h"
#include <cstdint>

class SrvManager;


class PostProcess {
public:
	static PostProcess* GetInstance();
	static void Finalize();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Draw(uint32_t textureSrvIndex);

private:
	void CreateRootSignature();
	void CreateGraphicsPipelineState();

private:
	static PostProcess* instance;

	PostProcess() = default;
	~PostProcess() = default;
	PostProcess(PostProcess&) = delete;
	PostProcess& operator=(PostProcess&) = delete;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};