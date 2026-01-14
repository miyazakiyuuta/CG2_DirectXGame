#pragma once
#include "DirectXCommon.h"
#include "engine/base/Matrix4x4.h"

class ModelCommon;

class Model {
public: // メンバ関数
	void Initialize(ModelCommon* modelCommon, const std::string& directorypath, const std::string& filename);
	void Draw();

private: // メンバ構造体
	struct Material {
		MatrixMath::Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
		float padding1[3];
	};

	struct MaterialData {
		std::string textureFilePath;
		uint32_t textureIndex = 0;
	};

	// 頂点データ
	struct VertexData {
		MatrixMath::Vector4 position;
		MatrixMath::Vector2 texcoord;
		MatrixMath::Vector3 normal;
	};

	struct ModelData {
		std::vector<VertexData> vertices;
		MaterialData material;
	};

private: // メンバ関数
	// .mtlファイルの読み取り
	static MaterialData LoadMaterialTemplateFile(const std::string& directoryPath, const std::string& filename);
	// .objファイルの読み取り
	static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);

	void CreateVertexData();
	void CreateMaterialData();

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

