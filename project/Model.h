#pragma once
#include "DirectXCommon.h"
#include "engine/base/Matrix4x4.h"
#include "engine/base/Vector2.h"
#include "engine/base/Vector3.h"
#include "engine/base/Vector4.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

class ModelCommon;

class Model {
public: // メンバ関数
	void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
	void Draw();

private: // メンバ構造体
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
		Matrix4x4 localMatrix;
		std::string name;
		std::vector<Node> children;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
		Node rootNode;
	};

public:
	ModelData GetModelData() { return modelData_; }

private: // メンバ関数
	// .mtlファイルの読み取り
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// .objファイルの読み取り
	static ModelData LoadModelFile(const std::string& directoryPath, const std::string& filename);

	void CreateVertexData();
	void CreateMaterialData();

	static Node ReadNode(aiNode* node);

private: // メンバ変数
	ModelCommon* modelCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	// Objファイルのデータ
	ModelData modelData_;

	// バッファリソース
	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;

	// バッファリソースの使い道を捕捉するバッファビュー
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;

	// バッファリソース内のデータを指すポインタ
	VertexData* vertexData_ = nullptr;
	Material* materialData_ = nullptr;
};

