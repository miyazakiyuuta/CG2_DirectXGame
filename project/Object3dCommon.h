#pragma once
#include "DirectXCommon.h"

// 3Dオブジェクト共通部
class Object3dCommon {
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

private: // メンバ変数
	DirectXCommon* dxCommon_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};

