#pragma once
#include "base/DirectXCommon.h"
#include "3d/Animation.h"
//#include "3d/Skeleton.h"
#include "math/Matrix4x4.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Transform.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <map>
#include <span>

class ModelCommon;

struct Skeleton;

class Model {
public: // メンバ関数
	void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
	void Update(const Skeleton& skeleton);
	void Draw();

	static Animation LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

	void CreateSkinCluster(const Skeleton& skeleton);

public: // メンバ構造体
	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
		float padding1[3];
	};

	struct MaterialData {
		std::string textureFilePath;
		uint32_t srvIndex = 0;
	};

	// 頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	struct Node {
		QuaternionTransform transform;
		Matrix4x4 localMatrix;
		std::string name;
		std::vector<Node> children;
	};

	struct VertexWeightData {
		float weight;
		uint32_t vertexIndex;
	};

	struct JointWeightData {
		Matrix4x4 inverseBindPoseMatrix;
		std::vector<VertexWeightData> vertexWeights;
	};

	struct ModelData {
		std::map<std::string, JointWeightData> skinClusterData;
		std::vector<VertexData> vertices;
		std::vector<uint32_t> indices;
		MaterialData material;
		Node rootNode;
	};

	static constexpr uint32_t kNumMaxInfluence = 4; // 影響
	struct VertexInfluence {
		std::array<float, kNumMaxInfluence> weights;
		std::array<int32_t, kNumMaxInfluence> jointIndices;
	};

	struct WellForGPU {
		Matrix4x4 skeletonSpaceMatrix; // 位置用
		Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 法線用
	};

	struct SkinCluster {
		std::vector<Matrix4x4> inverseBindPoseMatrices;
		Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
		D3D12_VERTEX_BUFFER_VIEW influenceBufferView;
		std::span<VertexInfluence> mappedInfluence;
		Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
		std::span<WellForGPU> mappedPalette;
		uint32_t paletteSrvIndex;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle;
	};

public:
	ModelData GetModelData() { return modelData_; }
	const Animation& GetAnimation() const { return animation_; }
	SkinCluster GetSkinCluster() { return skinCluster_; }

private: // メンバ関数
	// .mtlファイルの読み取り
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// .objファイルの読み取り
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	void CreateVertexData();
	void CreateMaterialData();
	void CreateIndexData();

	static Node ReadNode(aiNode* node);

private: // メンバ変数
	ModelCommon* modelCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	// Objファイルのデータ
	ModelData modelData_;

	Animation animation_;

	SkinCluster skinCluster_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;

	// バッファリソースの使い道を捕捉するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;

	// バッファリソース内のデータを指すポインタ
	std::span<VertexData> vertexData_;
	std::span<uint32_t> indexData_;
	Material* materialData_ = nullptr;
};

