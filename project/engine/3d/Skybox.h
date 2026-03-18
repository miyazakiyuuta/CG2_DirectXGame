#pragma once
#include "base/DirectXCommon.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"

class Camera;

class Skybox {
public:
	void Initialize(DirectXCommon* dxCommon, const std::string& textureFilePath);
	void Draw(const Camera& camera);

private:
	struct Vertex {
		Vector4 position;
	};

	struct TransformationMatrix {
		Matrix4x4 WVP;
	};

	void CreateMesh();
	void CreateGraphicsPipeline();

private:
	DirectXCommon* dxCommon_ = nullptr;
	std::string textureFilePath_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};
	Microsoft::WRL::ComPtr<ID3D12Resource> constantBufferResource_;
	TransformationMatrix* constantBufferData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};

