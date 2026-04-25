#pragma once
#include "effect/Particle.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "3d/Camera.h"
#include <d3d12.h>
#include <wrl.h>

class GPUParticleEmitter {
public:
    void Initialize();
	void Update(float deltaTime);
    void Draw(Camera* camera);

    void SetTexture(uint32_t srvIndex) { textureSrvIndex_ = srvIndex; }

private:

    struct Material {
        Vector4 color;
        int enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };


    void CreateResource();
    void CreateCSPipelineState();
    void CreateEmitCSPipelineState();
    void CreateDrawPipelineState();
    void CreatePerViewBuffer();
    void CreateMaterialBuffer();
    void CreateEmitterBuffer();
	void CreatePerFrameBuffer();
    void InitializeParticles(); // CSで初期化
	void EmitParticles(); // CSで射出

    Microsoft::WRL::ComPtr<ID3D12Resource> particleResource_;

    uint32_t srvIndex_ = 0;
    uint32_t uavIndex_ = 0;
    uint32_t freeCounterUavIndex_ = 0;
    uint32_t textureSrvIndex_ = 0;

    static const uint32_t kMaxParticles = 1024;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureCS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateCS_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureDraw_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateDraw_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignatureEmitCS_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStateEmitCS_;

    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;
    PerView* perViewData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource_;
    EmitterSphere* emitterSphere_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;
    PerFrame* perFrameData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> freeCounterResource_;
};

