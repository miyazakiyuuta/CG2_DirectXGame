#pragma once
#include "base/DirectXCommon.h"
#include "3d/Camera.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"

class DebugGrid {
public:
	struct Vertex {
		Vector3 pos;
	};
	struct ConstBufferData {
		Matrix4x4 matWVP;
		Vector4 color;
	};
	void Initialize(DirectXCommon* dxCommon, float gridSize = 10.0f, uint32_t subdivision = 10);
	void Draw(const Camera& camera);

	void SetPosition(Vector3 pos) { position_ = pos; }
	void SetColor(Vector4 color) { color_ = color; }

private:
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	Microsoft::WRL::ComPtr<ID3D12Resource> constantBufferResource_;
	ConstBufferData* mappedConstantData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	uint32_t vertexCount_ = 0;

	Vector3 position_ = { 0.0f,0.0f,0.0f };
	Vector4 color_ = { 0.5f,0.5f,0.5f,0.0f };
};

