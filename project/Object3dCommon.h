#pragma once
#include "DirectXCommon.h"
#include "Camera.h"
#include "engine/base/Vector3.h"
#include "engine/base/Vector4.h"

struct PointLight {
	Vector4 color; //!< ライトの色
	Vector3 position; //!< ライトの位置
	float intensity; //!< 輝度
	float radius; //!< ライトの届く最大距離
	float decay; //!< 減衰率
};

struct SpotLight {
	Vector4 color; //!< ライトの色
	Vector3 position; //!< ライトの位置
	float intensity; //!< 輝度
	Vector3 direction; //!< スポットライトの方向
	float distance; //!< ライトの届く最大距離
	float decay; //!< 減衰率
	float cosAngle; //!< スポットライトの余弦
	float cosFalloffStart;
	float padding;
};

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
	void SetPointLight(const PointLight& light) { *pointLightData_ = light; }
	void SetSpotLight(const SpotLight& light) { *spotLightData_ = light; }
	// getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }
	D3D12_GPU_VIRTUAL_ADDRESS GetPointLightGPUAddress() const { return pointLightResource_->GetGPUVirtualAddress(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightGPUAddress() const { return spotLightResource_->GetGPUVirtualAddress(); }

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	// グラフィクスパイプラインの生成
	void CreateGraphicsPipelineState();

	void InitializePointLight();

	void InitializeSpotLight();

private:
	static Object3dCommon* instance;

private: // メンバ変数
	DirectXCommon* dxCommon_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Camera* defaultCamera_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	PointLight* pointLightData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	SpotLight* spotLightData_ = nullptr;
};

