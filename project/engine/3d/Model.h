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
	// 描画。スキン済み頂点(個体別)を使う場合はそのVBVを渡す
	void Draw(const D3D12_VERTEX_BUFFER_VIEW* overrideVertexBufferView = nullptr);

	static std::map<std::string, Animation> LoadAnimationFile(const std::string& directoryPath, const std::string& filename);

public: // メンバ構造体
	

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

	// スキニング資産のうち「全インスタンスで共有できるもの」。
	// 形(入力頂点)・骨と頂点の対応(インフルエンス)・バインドポーズはモデル固有なので1つでよい
	struct SkinClusterShared {
		std::vector<Matrix4x4> inverseBindPoseMatrices;

		uint32_t inputVertexSrvIndex = 0;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> inputVertexSrvHandle{};

		Microsoft::WRL::ComPtr<ID3D12Resource> influenceResource;
		std::span<VertexInfluence> mappedInfluence;
		uint32_t influenceSrvIndex = 0;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> influenceSrvHandle{};

		// 頂点数(モデル定数)を渡すCB
		Microsoft::WRL::ComPtr<ID3D12Resource> skinningInformationResource;

		bool built = false; // EnsureSkinClusterSharedの冪等化用
	};

	// スキニング資産のうち「個体(Object3d)ごとに必要なもの」。
	// ポーズ(パレット)とスキン済み頂点は個体で異なるため共有すると全員が同じポーズになってしまう
	struct SkinClusterInstance {
		Microsoft::WRL::ComPtr<ID3D12Resource> paletteResource;
		std::span<WellForGPU> mappedPalette;
		uint32_t paletteSrvIndex = 0;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> paletteSrvHandle{};

		Microsoft::WRL::ComPtr<ID3D12Resource> skinnedVertexResource;
		uint32_t uavIndex = 0;
		std::pair<D3D12_CPU_DESCRIPTOR_HANDLE, D3D12_GPU_DESCRIPTOR_HANDLE> uavHandle{};

		bool created = false;
		bool srvAllocated = false; // SRVスロットは一度確保したら使い回す(SetModelし直してもリークしない)
	};

	struct SkinningInformation { uint32_t numVertices; };

public:
	// 共有スキニング資産を(未構築なら)構築する。構築済みなら何もしない
	void EnsureSkinClusterShared(const Skeleton& skeleton);
	// 個体用スキニング資産を構築する。instanceは呼び出し側(Object3d)が所有する
	void CreateSkinClusterInstance(SkinClusterInstance& instance, const Skeleton& skeleton);

	// ボーンウェイトを持つモデルか
	bool HasSkin() const { return !modelData_.skinClusterData.empty(); }

	const ModelData& GetModelData() const { return modelData_; }
	const std::map<std::string, Animation>& GetAnimations() const { return animations_; }
	const SkinClusterShared& GetSkinClusterShared() const { return skinClusterShared_; }

private: // メンバ関数
	// .mtlファイルの読み取り
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// .objファイルの読み取り
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	void CreateVertexData();
	void CreateIndexData();

	static Node ReadNode(aiNode* node);

private: // メンバ変数
	ModelCommon* modelCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	// Objファイルのデータ
	ModelData modelData_;

	std::map<std::string, Animation> animations_;

	SkinClusterShared skinClusterShared_;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
	std::span<VertexData> vertexData_;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	D3D12_INDEX_BUFFER_VIEW indexBufferView_;
	std::span<uint32_t> indexData_;
};

