#pragma once
#include "base/DirectXCommon.h"
#include "3d/Camera.h"
#include "math/Vector3.h"
#include "math/Vector4.h"

static const uint32_t kMaxPointLights = 16;

struct PointLight {
	Vector4 color; //!< ライトの色
	Vector3 position; //!< ライトの位置
	float intensity; //!< 輝度
	float radius; //!< ライトの届く最大距離
	float decay; //!< 減衰率
	float padding[2];
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

struct PointLightArray {
	PointLight lights[kMaxPointLights];
	uint32_t count;
	float padding[3];
};

class SrvManager;

// 3Dオブジェクト共通部
class Object3dCommon {
public:
	static Object3dCommon* GetInstance();


public: // メンバ関数
	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	// 共通描画設定 (PreDraw)
	void CommonDrawSetting();
	void SkinningDrawSetting();
	void ComputeDispatchSetting();

public:
	// setter
	void SetDefaultCamera(Camera* camera) { defaultCamera_ = camera; }
	void SetPointLights(const PointLightArray& lights) { *pointLightData_ = lights; }
	void SetSpotLight(const SpotLight& light) { *spotLightData_ = light; }
	void SetEnvironmentSrvIndex(uint32_t srvIndex) { environmentSrvIndex_ = srvIndex; }
	// getter
	DirectXCommon* GetDxCommon() const { return dxCommon_; }
	Camera* GetDefaultCamera() const { return defaultCamera_; }
	D3D12_GPU_VIRTUAL_ADDRESS GetPointLightGPUAddress() const { return pointLightResource_->GetGPUVirtualAddress(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetSpotLightGPUAddress() const { return spotLightResource_->GetGPUVirtualAddress(); }
	SrvManager* GetSrvManager() { return srvManager_; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetEnvironmentSrvHandle() const;

private:
	// ルートシグネチャの作成
	void CreateRootSignature();
	void CreateSkinningRootSignature();
	void CreateComputeRootSignature();
	// グラフィクスパイプラインの生成
	void CreateGraphicsPipelineState();
	void CreateSkinningGraphicsPipelineState();
	void CreateComputePipelineState();

	void InitializePointLight();

	void InitializeSpotLight();

private:
	static Object3dCommon* instance;

private: // メンバ変数
	DirectXCommon* dxCommon_;
	SrvManager* srvManager_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> skinningRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> skinningPipelineState_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> computeRootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> computePipelineState_ = nullptr;

	Camera* defaultCamera_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_;
	PointLightArray* pointLightData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_;
	SpotLight* spotLightData_ = nullptr;

	uint32_t environmentSrvIndex_ = 0;
};

