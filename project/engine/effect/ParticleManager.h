#pragma once
#include "base/DirectXCommon.h"
#include "math/Matrix4x4.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Transform.h"
#include "effect/ParticleConfig.h"
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

	void CreateParticleGroup(const std::string name, const std::string textureFilePath, BlendMode blendMode = BlendMode::Alpha);

	void Emit(const std::string name, const Vector3& position, const ParticleConfig& config, uint32_t count);

	void SetCamera(const Camera* camera) { camera_ = camera; }

private:
	struct MaterialData {
		std::string textureFilePath;
		uint32_t srvIndex = 0;
	};

	struct Particle {
		Transform transform;
		Vector3 velocity;
		Vector4 color;
		float lifeTime;
		float currentTime;
		ParticleMoveType moveType;

		Vector3 baseScale{};       // 寿命スケール変化の基準(生成時スケール)
		Vector3 gravity{};
		float   drag = 0.0f;
		float   startScaleMul = 1.0f;
		float   endScaleMul = 1.0f;
		bool    useColorLerp = false;
		Vector4 startColor{};
		Vector4 endColor{};

		bool colorEaseIn = false;

		float swayStrength = 0.0f;
		float swayFrequency = 0.0f;
		float swayPhase = 0.0f;
	};

	struct InstanceData {
		Matrix4x4 wvp;
		Matrix4x4 world;
		Vector4 color;
	};

	struct ParticleGroup {
		MaterialData material;
		std::list<Particle> particles;
		uint32_t instancingSrvIndex = 0;
		Microsoft::WRL::ComPtr<ID3D12Resource> instancingResource;
		uint32_t instanceCount = 0;
		InstanceData* instancingData = nullptr; // 書き込み先
		BlendMode blendMode = BlendMode::Alpha;
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

private:
	void CreateInstancingResource(ParticleGroup& group);

	void CreateRootSignature();

	void CreateGraphicsPipelineState(BlendMode blendMode);

	void CreateVertexResource();

private:

	static ParticleManager* instance;

	ParticleManager() = default;
	ParticleManager(ParticleManager&) = delete;
	ParticleManager& operator=(ParticleManager&) = delete;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	std::unordered_map<BlendMode, Microsoft::WRL::ComPtr<ID3D12PipelineState>> pipelineStates_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	Microsoft::WRL::ComPtr< ID3D12Resource> materialResource_;
	MaterialForGPU* materialData_ = nullptr;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;

	//const uint32_t kNumMaxInstance = 1024;
	const uint32_t kNumMaxInstance = 4096;

	std::unordered_map<std::string, ParticleGroup> particleGroups_;

};

