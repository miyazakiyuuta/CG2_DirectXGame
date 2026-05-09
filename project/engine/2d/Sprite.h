#pragma once
#include "base/DirectXCommon.h"
#include "math/Matrix4x4.h"
#include "math/Vector2.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include <string>

class SpriteCommon;
class SrvManager;

// スプライト
class Sprite {
public: // メンバ関数
	void Initialize(SpriteCommon* spriteCommon, std::string textureFilePath);
	void Update();
	void Draw();

public:
	// getter
	const Vector2& GetPos() const { return pos_; }
	float GetRotation() const { return rotation_; }
	const Vector4& GetColor() const { return materialData_->color; }
	const Vector2& GetSize() const { return size_; }
	const Vector2& GetAnchorPoint() const { return anchorPoint_; }

	// setter
	void SetPos(const Vector2& pos) { pos_ = pos; }
	void SetRotation(float rotation) { rotation_ = rotation; }
	void SetColor(const Vector4& color) { materialData_->color = color; }
	void SetSize(const Vector2& size) { size_ = size; }
	void SetAnchorPoint(const Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
	void SetTextureLeftTop(const Vector2& leftTop) { textureLeftTop_ = leftTop; }
	void SetTextureSize(const Vector2& size) { textureSize_ = size; }

private:
	void CreateVertexData();
	void CreateIndexData();
	void CreateMaterialData();
	void CreateTransformationMatrixData();
	void AdjustTextureSize();

private:
	// 頂点データ
	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	// マテリアルデータ：シェーダー(Sprite.PS.hlsl)の定義と完全に一致させる
	// 順番が異なるとUV計算が壊れます
	struct Material {
		Vector4 color;         // 16バイト
		Matrix4x4 uvTransform; // 64バイト
		// ※ enableLightingなどはシェーダー側で定義されていないため削除
	};

	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

	SpriteCommon* spriteCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;

	VertexData* vertexData_ = nullptr;
	uint32_t* indexData_ = nullptr;
	Material* materialData_ = nullptr;
	TransformationMatrix* transformationMatrixData_ = nullptr;

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	Vector2 pos_ = {0.0f, 0.0f};
	float rotation_ = 0.0f;
	Vector2 size_ = {1.0f, 1.0f};
	Vector2 anchorPoint_ = {0.0f, 0.0f};

	bool isFlipX_ = false;
	bool isFlipY_ = false;

	Vector2 textureLeftTop_ = {0.0f, 0.0f};
	Vector2 textureSize_ = {100.0f, 100.0f};

	std::string filePath_;
	uint32_t srvIndex_ = 0;
};