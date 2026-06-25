// effect/DepthBasedOutline.h
#pragma once
#include "effect/IPostEffect.h"
#include "math/Matrix4x4.h"
#include "math/Vector3.h"
#include <wrl/client.h>
#include <d3d12.h>

class Camera;

class DepthBasedOutline : public IPostEffect {
public:
    DepthBasedOutline() { name = "DepthBasedOutline"; }

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Update(float deltaTime) override;
    void Draw(uint32_t srcSrvIndex) override;
    void DrawImGui() override;

    void OnResize();

    // 毎フレームのprojInverseはCameraから自動取得（ポインタは1回設定すればOK）
    void SetCamera(const Camera* camera) { camera_ = camera; }
    // Cameraを使わず直接渡したい場合
    void SetProjectionInverse(const Matrix4x4& m);

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    struct OutlineParam {     // HLSL側と一致（96byte）
        Matrix4x4 projectionInverse; // 64
        Vector3   lineColor;         // 12
        float     strength;          //  4
        float     threshold;         //  4
        int32_t   debugView;         //  4
        float     pad0, pad1;        //  8
    };

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
    Microsoft::WRL::ComPtr<ID3D12Resource>      paramResource_;
    OutlineParam* paramData_ = nullptr;

    uint32_t      depthSrvIndex_ = 0;
    const Camera* camera_ = nullptr;

    // ImGui調整値
    Vector3 lineColor_ = { 0.0f, 0.0f, 0.0f };
    float   strength_ = 1.0f;
    float   threshold_ = 0.0f;
    int     debugView_ = 0;
};