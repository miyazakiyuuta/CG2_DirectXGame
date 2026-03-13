#pragma once
#include "base/DirectXCommon.h"
#include "3d/Camera.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include <vector>

class DebugSphere {
public:
	struct Vertex {
		Vector3 pos;
	};

	struct TransformationMatrix {
		Matrix4x4 matWVP;
		Vector4 color;
	};

	struct ConstBufferData {
		TransformationMatrix data[128];
	};

	void Initialize(DirectXCommon* dxCommon);
	void Draw(const std::vector<Vector3>& centers, float radius, const Vector4& color, const Camera& camera);

private:
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBufferResource_;
	ConstBufferData* mappedConstantData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

	uint32_t indexCount_ = 0;
};