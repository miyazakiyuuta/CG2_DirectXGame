#pragma once
#include "base/DirectXCommon.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include "math/Transform.h"

class SrvManager;
class Camera;

struct CylinderConfig {
	float lifeTime = 1.0f;
	float startScale = 1.0f;
	float endScale = 1.5f;
	Vector4 startColor = { 1.0f,1.0f,1.0f,1.0f };
	Vector4 endColor = { 1.0f,1.0f,1.0f,0.0f };
};

class CylinderManager {
public:
	static CylinderManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
	void Update(float deltaTime);
	void Draw();

	void Emit(const Vector3& position, const CylinderConfig& config);

	void SetCamera(const Camera* camera) { camera_ = camera; }

private:
	struct CylinderData {
		Transform transform;
		CylinderConfig config;
		float currentTime;
	};

	struct InstanceData {
		Matrix4x4 wvp;
		Matrix4x4 world;
		Vector4 color;
	};

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

	static CylinderManager* instance;

	CylinderManager() = default;
	~CylinderManager() = default;
	CylinderManager(CylinderManager&) = delete;
	CylinderManager& operator=(CylinderManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;

	std::vector<CylinderData> cylinders_;

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

	float uvOffsetU = 0.0f;
	float uvOffsetV = 0.0f;
};

