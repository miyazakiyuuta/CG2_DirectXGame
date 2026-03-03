#pragma once
#include "base/DirectXCommon.h"
#include "math/Matrix4x4.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Transform.h"

#include <string>
#include <vector>
#include <memory>

class SrvManager;
class Camera;

struct RingConfig {
	float lifeTime = 0.5f;
	float startScale = 0.1f;
	float endScale = 5.0f;
	Vector4 startColor = { 1.0f,1.0f,1.0f,1.0f };
	Vector4 endColor = { 1.0f,1.0f,1.0f,0.0f };
	bool isBillboard = false;
};

class RingManager {
public:
	static RingManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(float deltaTime);
	void Draw();

	void Emit(const Vector3& position, const Vector3& rotation, const RingConfig& config);

	void SetCamera(const Camera* camera) { camera_ = camera; }

private:
	// 1つのリングのデータ
	struct RingData {
		Transform transform;
		RingConfig config;
		float currentTime;
	};
	// インスタンシング描画用
	struct InstanceData {
		Matrix4x4 wvp;
		Matrix4x4 world;
		Vector4 color;
	};
	// 頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct MaterialForGPU {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	void CreateRootSignature();

	void CreateGraphicsPipelineState();

	void CreateVertexResource();

private:
	static RingManager* instance;

	RingManager() = default;
	~RingManager() = default;
	RingManager(RingManager&) = delete;
	RingManager& operator=(RingManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;

	std::vector<RingData> rings_;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	MaterialForGPU* materialData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource_;
	InstanceData* instancingData_ = nullptr;
	uint32_t instancingSrvIndex_ = 0;

	uint32_t textureSrvIndex_ = 0;

	static const uint32_t kNumMaxInstance = 100;
	uint32_t numInstance_ = 0;
};

