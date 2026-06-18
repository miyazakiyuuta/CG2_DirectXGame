#pragma once
#include "effect/IPostEffect.h"

#include "math/Vector3.h"

#include <wrl/client.h>
#include <d3d12.h>

class Monochrome : public IPostEffect {
public:
    Monochrome() { name = "Monochrome"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Draw(uint32_t srcSrvIndex) override;

    void DrawImGui() override;

	void SetColor(const Vector3& color) {
		paramData_->color = color;
	}

	void SetIntensity(float intensity) {
		paramData_->intensity = intensity;
	}

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    struct MonochromeParam {
        Vector3 color;
        float intensity;
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
    MonochromeParam* paramData_ = nullptr;
};