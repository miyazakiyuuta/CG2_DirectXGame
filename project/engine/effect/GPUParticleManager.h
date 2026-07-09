#pragma once
#include "base/DirectXCommon.h"
#include "effect/GPUParticle.h"
#include "effect/GPUParticleConfig.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <wrl.h>

class SrvManager;
class Camera;
class GPUParticleEmitter;

// GPUパーティクルの共有資源(RootSignature/PSO)とグループ(粒子バッファ+エミッタ)を管理するシングルトン。
// 生成(Emit CS)・更新(Update CS)・描画まで粒子データはVRAM内で完結し、CPUはパラメータ書き込みとDispatchだけ行う。
// Initialize/Update/DispatchAll/Draw/FinalizeはFrameworkが呼ぶので、シーンはSetCamera(camera)を渡すだけ。
// ブレンドはAdd固定(加算合成は描画順非依存のためGPUソートが不要になる)
class GPUParticleManager {
public:
	// 1グループあたりの最大粒子数(GPUParticle.hlsliのkMaxParticlesと必ず一致させる)
	static const uint32_t kMaxParticles = 102400;

	static GPUParticleManager* GetInstance();

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	void Finalize();

	// エミッタパラメータ・PerFrameのCBV書き込みと射出タイミング判定(Frameworkが毎フレーム呼ぶ)
	void Update(float deltaTime);

	// 全グループのEmit/Update CSをまとめてDispatchする(Frameworkがシーン描画前に呼ぶ)
	void DispatchAll();

	// 加算合成なのでシーン描画の後に呼ぶ(Frameworkが毎フレーム呼ぶ)
	void Draw();

	// グループを作成する。既存の名前なら何もしない(冪等)
	void CreateParticleGroup(const std::string& name, const std::string& textureFilePath, const GPUParticleConfig& config);

	void DrawImGui();

	// GPUParticleConfig編集用の共通UI(USE_IMGUI未定義時は何もしない)
	static void DrawConfigImGui(GPUParticleConfig& config);

	void SetCamera(const Camera* camera) { camera_ = camera; }

	// GPUParticleEmitterがコンストラクタ/デストラクタで自動的に呼ぶ(直接呼ぶ必要はない)
	void RegisterEmitter(GPUParticleEmitter* emitter);
	void UnregisterEmitter(GPUParticleEmitter* emitter);

private:
	struct ParticleGroup {
		// 粒子・フリーリスト(いずれもDEFAULTヒープのUAVバッファ。CPUからは触らない)
		Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> freeListIndexResource;
		Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;
		// UAV3つは1つのDescriptorTable(u0:粒子 u1:freeListIndex u2:freeList)で渡すため
		// ヒープ上で連続していること(particleUavIndexから3つ)
		uint32_t particleUavIndex = 0;
		uint32_t particleSrvIndex = 0; // 描画時にVSから読むSRV

		// エミッタ・configのCBV(Mapしっぱなし。毎フレームUpdateで書き込む)
		Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;
		EmitterSphere* emitterData = nullptr;
		Microsoft::WRL::ComPtr<ID3D12Resource> configResource;
		GPUParticleConfigForGPU* configData = nullptr;

		std::string textureFilePath;
		uint32_t textureSrvIndex = 0;
		bool needsInitialize = true; // 次のDispatchAllでInitialize CSを流す
	};

	struct MaterialForGPU {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> CreateUavBuffer(uint64_t sizeInBytes);

	void CreateInitCSPipeline();
	void CreateEmitCSPipeline();
	void CreateUpdateCSPipeline();
	void CreateDrawPipeline();
	void CreateSharedBuffers();

	// 粒子3バッファのUAV書き込み完了を待つハザードバリア
	void BarrierUAV(ID3D12GraphicsCommandList* commandList, ParticleGroup& group);

	static void WriteConfig(ParticleGroup& group, const GPUParticleConfig& config);

private:
	static GPUParticleManager* instance;

	GPUParticleManager() = default;
	GPUParticleManager(GPUParticleManager&) = delete;
	GPUParticleManager& operator=(GPUParticleManager&) = delete;

	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	const Camera* camera_ = nullptr;

	// CSのスレッドグループサイズ(numthreadsと一致させる)
	static const uint32_t kThreadGroupSize = 1024;

	// 全グループ共有のRootSignature・PSO
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureInitCS_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateInitCS_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureEmitCS_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateEmitCS_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureUpdateCS_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateUpdateCS_;
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureDraw_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateDraw_;

	// 全グループ共有のCBV
	Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
	PerView* perViewData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
	PerFrame* perFrameData_ = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	MaterialForGPU* materialData_ = nullptr;

	std::unordered_map<std::string, ParticleGroup> particleGroups_;

	std::vector<GPUParticleEmitter*> emitters_; // 生成中のエミッタ(所有はしない)
};
