#pragma once
#include "DirectXCommon.h"

// スプライト共通部
class SpriteCommon {
public: // メンバ関数
	void Initialize(DirectXCommon* dxCommon);
	// 共通描画設定 (PreDraw)
	void CommonDrawSetting();

public: // getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィクスパイプラインの生成
	void CreateGraphicsPipelineState();

private:
	DirectXCommon* dxCommon_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};

