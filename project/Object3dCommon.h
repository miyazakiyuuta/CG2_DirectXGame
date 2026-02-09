#pragma once
#include "DirectXCommon.h"
#include "Camera.h"

// 3Dオブジェクト共通部
class Object3dCommon {
public:
	static Object3dCommon* GetInstance();


public: // メンバ関数
	void Initialize(DirectXCommon* dxCommon);
	// 共通描画設定 (PreDraw)
	void CommonDrawSetting();

public:
	// setter
	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
	// getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィクスパイプラインの生成
	void CreateGraphicsPipelineState();

private:
	static Object3dCommon* instance;

private: // メンバ変数
	DirectXCommon* dxCommon_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Camera* defaultCamera_ = nullptr;
};

