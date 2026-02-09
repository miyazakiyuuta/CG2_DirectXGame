#pragma once
#include "DirectXCommon.h"

class SrvManager;

// スプライト共通部
class SpriteCommon {
public:
	static SpriteCommon* GetInstance();

public: // メンバ関数
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	// 共通描画設定 (PreDraw)
	void CommonDrawSetting();

public: // getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	SrvManager* GetSrvManager() const { return srvManager_; }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィクスパイプラインの生成
	void CreateGraphicsPipelineState();

private:
	static SpriteCommon* instance;

private:
	DirectXCommon* dxCommon_;

	SrvManager* srvManager_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};

