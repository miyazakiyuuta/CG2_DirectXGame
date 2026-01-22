#pragma once
#include "DirectXCommon.h"
#include "engine/base/Matrix4x4.h"
#include "engine/base/Vector3.h"
#include <string>
#include <random>

class SrvManager;
class Camera;

class ParticleManager {
public:
	static ParticleManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Update(float deltaTime);

	void Draw();

	void CreateParticleGroup(const std::string name, const std::string textureFilePath);

	void Emit(const std::string name, const MatrixMath::Vector3& position, uint32_t count);

	void SetCamera(const Camera* camera) { camera_ = camera; }

private:
	struct MaterialData {
		std::string textureFilePath;
		uint32_t srvIndex = 0;
	};

	struct Particle {
		MatrixMath::Transform transform;
		MatrixMath::Vector3 velocity;
		MatrixMath::Vector4 color;
		float lifeTime;
		float currentTime;
	};

	/* 一粒ごとの場合
	struct ParticleForGPU {
		Matrix4x4 WVP;
		Matrix4x4 World;
		MatrixMath::Vector4 color;
	};
	*/

	struct InstanceData {
		Matrix4x4 wvp;
		Matrix4x4 world;
		MatrixMath::Vector4 color;
	};

	struct ParticleGroup {
		MaterialData material;
		std::list<Particle> particles;
		uint32_t instancingSrvIndex = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
		uint32_t instanceCount = 0;
		InstanceData* instancingData = nullptr; // 書き込み先
	};

	// 頂点データ
	struct VertexData {
		MatrixMath::Vector4 position;
		MatrixMath::Vector2 texcoord;
		MatrixMath::Vector3 normal;
	};

	struct MaterialForGPU {
		MatrixMath::Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

private:
	void CreateInstancingResource(ParticleGroup& group);

	void CreateRootSignature();

	void CreateGraphicsPipelineState();

	void CreateVertexResource();

private:

	static ParticleManager* instance;

	ParticleManager() = default;
	ParticleManager(ParticleManager&) = delete;
	ParticleManager& operator=(ParticleManager&) = delete;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	Microsoft::WRL::ComPtr< ID3D12Resource> materialResource_;
	MaterialForGPU* materialData_ = nullptr;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;

	const uint32_t kNumMaxInstance = 1024;

	std::unordered_map<std::string, ParticleGroup> particleGroups_;

	std::random_device seedGenerator;
	std::mt19937 randomEngine{ seedGenerator() };
};

