#pragma once
#include "base/DirectXCommon.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Matrix4x4.h"
#include "3d/Camera.h"

#include <vector>

class DebugRenderer {
public:

	static DebugRenderer* GetInstance();

	void Initialize(DirectXCommon* dxCommon);

	void AddSphere(const Vector3& center, float radius, const Vector4& color = { 1.0f,1.0f,1.0f,1.0f });
	void AddLine(const Vector3& start, const Vector3& end, const Vector4& color = { 0.5f,0.5f,0.5f,1.0f });
	void AddGrid(const Vector3& pos, float gridSize = 10.0f, uint32_t subdivision = 10, const Vector4& color = { 0.5f,0.5f,0.5f,1.0f });
	void AddBox3D(const Vector3& center, const Vector3& size, const Vector4& color = { 1.0f,1.0f,1.0f,1.0f });
	void AddBox3DSolid(const Vector3& center, const Vector3& size, const Vector4& color = { 1.0f,1.0f,1.0f,1.0f });
	void AddBox2D(const Vector2& leftTop, const Vector2& size, const Vector4& color = { 1.0f,1.0f,1.0f,1.0f });

	void RenderAll(const Camera& camera);

private:

	struct Vertex {
		Vector3 pos;
		Vector2 uv;
		Vector4 color;
	};

	struct ConstBufferData {
		struct TransformationMatrix {
			Matrix4x4 matWVP;
			Vector4 color;
			float padding[44];
		} data[128];
	};

	struct SphereRequest {
		Vector3 center;
		float radius;
		Vector4 color;
	};

	struct Box3DSolidRequest {
		Vector3 center;
		Vector3 size;
		Vector4 color;
	};

	struct Box2DRequest {
		Vector2 leftTop;
		Vector2 size;
		Vector4 color;
	};

	struct Mesh {
		Microsoft::WRL::ComPtr<ID3D12Resource> vertexBufferResource;
		D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
		Microsoft::WRL::ComPtr<ID3D12Resource> indexBufferResource;
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		uint32_t indexCount;
	};

	void CreateSphereMesh();
	void CreateBox3DMesh();
	void CreateBox2DMesh();

private:

	DebugRenderer() = default;
	static DebugRenderer* instance;

	// 固定形状
	std::vector<SphereRequest> sphereRequests_;
	Mesh sphereMesh_;
	std::vector<Box3DSolidRequest> box3DSolidRequests_;
	Mesh box3DMesh_;
	std::vector<Box2DRequest> box2DRequests_;
	Mesh box2DMesh_;
	// 動的形状
	std::vector<Vertex> lineVertices_;
	Microsoft::WRL::ComPtr<ID3D12Resource> lineBuffer_;
	D3D12_VERTEX_BUFFER_VIEW lineBufferView_;

	static const uint32_t kMaxLineVertices = 100000;

	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3DLine_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso3DSolid_;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pso2DSolid_;

	Microsoft::WRL::ComPtr<ID3D12Resource> constBuffer_;
	ConstBufferData* mappedData_ = nullptr;



};

