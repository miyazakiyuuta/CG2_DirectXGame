#pragma once
#include "base/DirectXCommon.h"
#include "math/Matrix4x4.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Transform.h"
#include "effect/ParticleConfig.h"
#include <string>
#include <list>
#include <vector>
#include <unordered_map>

class SrvManager;
class Camera;
class ParticleEmitter;

class ParticleManager {
public:
	static ParticleManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Finalize();

	// 登録済みエミッタの更新と全パーティクルのシミュレーション(Frameworkが毎フレーム呼ぶ)
	void Update(float deltaTime);

	// 半透明のためシーン描画の後に呼ぶ(Frameworkが毎フレーム呼ぶ)
	void Draw();

	// グループを作成する。既存の名前なら何もしない(冪等)
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath, BlendMode blendMode = BlendMode::Alpha);

	// グループ作成と既定configの登録をまとめて行う
	void RegisterEffect(const std::string& name, const std::string& textureFilePath, const ParticleConfig& config, BlendMode blendMode = BlendMode::Alpha);

	// configを指定して発生させる
	void Emit(const std::string& name, const Vector3& position, const ParticleConfig& config, uint32_t count);

	// RegisterEffectで登録した既定configで発生させる
	void Emit(const std::string& name, const Vector3& position, uint32_t count);

	void DrawImGui();

	// ParticleConfig編集用の共通UI(USE_IMGUI未定義時は何もしない)
	static void DrawConfigImGui(ParticleConfig& config);

	void SetCamera(const Camera* camera) { camera_ = camera; }

	// ParticleEmitterがコンストラクタ/デストラクタで自動的に呼ぶ(直接呼ぶ必要はない)
	void RegisterEmitter(ParticleEmitter* emitter);
	void UnregisterEmitter(ParticleEmitter* emitter);

private:
	struct MaterialData {
		std::string textureFilePath;
		uint32_t srvIndex = 0;
	};

	struct Particle {
		Transform transform;
		Vector3 baseScale;       // 初期スケール(寿命補間の基準)
		Vector3 velocity;
		Vector3 acceleration;
		Vector4 startColor;
		Vector4 endColor;
		float endScaleRatio = 1.0f;
		float lifeTime = 0.0f;
		float currentTime = 0.0f;
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
		ParticleConfig defaultConfig{}; // RegisterEffectで登録される既定config
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

	const uint32_t kNumMaxInstance = 1024;

	std::unordered_map<std::string, ParticleGroup> particleGroups_;

	std::vector<ParticleEmitter*> emitters_; // 生成中のエミッタ(所有はしない)

};
