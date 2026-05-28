#pragma once
#include "base/DirectXCommon.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include "math/Transform.h"
#include <string>

class SrvManager;
class Camera;

class CylinderSkybox {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, const std::string& texturePath);
    void Update();
    void Draw();

    void SetCamera(const Camera* camera) { camera_ = camera; }
    Transform& GetTransform() { return transform_; }

private:
    struct TransformationMatrix {
        Matrix4x4 wvp;
        Matrix4x4 world;
    };

    struct VertexData {
        Vector4 position;
        Vector2 texcoord;
        Vector3 normal;
    };

    struct MaterialForGPU {
        Vector4 color;
        Matrix4x4 uvTransform;
    };

    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateVertexResource();

private:

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    const Camera* camera_ = nullptr;

    Transform transform_{};

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformationMatrix* transformData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    MaterialForGPU* materialData_ = nullptr;

    uint32_t textureSrvIndex_ = 0;
    uint32_t vertexCount_ = 0;
};

