#pragma once
#include "DirectXCommon.h"
#include "engine/base/Vector3.h"
#include "engine/base/Matrix4x4.h"
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

    const MatrixMath::Vector2& GetPos()const { return pos_; }
    float GetRotation()const { return rotation_; }
    const MatrixMath::Vector4& GetColor() const { return materialData_->color; }
    const MatrixMath::Vector2& GetSize() const { return size_; }
    const MatrixMath::Vector2& GetAnchorPoint() const { return anchorPoint_; }

    // setter

    void SetPos(const MatrixMath::Vector2& pos) { pos_ = pos; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetColor(const MatrixMath::Vector4& color) { materialData_->color = color; }
    void SetSize(const MatrixMath::Vector2& size) { size_ = size; }
    void SetAnchorPoint(const MatrixMath::Vector2& anchorPoint) { anchorPoint_ = anchorPoint; }
	void SetFlipX(bool isFlipX) { isFlipX_ = isFlipX; }
	void SetFlipY(bool isFlipY) { isFlipY_ = isFlipY; }
    void SetTextureLeftTop(const MatrixMath::Vector2& leftTop) { textureLeftTop_ = leftTop; }
	void SetTextureSize(const MatrixMath::Vector2& size) { textureSize_ = size; }

private:
    void CreateVertexData();
    void CreateIndexData();
    void CreateMaterialData();
    void CreateTransformationMatrixData();
    // テクスチャサイズをイメージに合わせる
    void AdjustTextureSize();

private:
    // 頂点データ
    struct VertexData {
        MatrixMath::Vector4 position;
        MatrixMath::Vector2 texcoord;
        MatrixMath::Vector3 normal;
    };

    //マテリアルデータ
    struct Material {
        MatrixMath::Vector4 color;
        int32_t enableLighting;
        float padding[3];
        Matrix4x4 uvTransform;
    };

    // 座標変換行列データ
    struct TransformationMatrix {
        Matrix4x4 WVP;
        Matrix4x4 World;
        Matrix4x4 WorldInverseTranspose;;
    };

	SpriteCommon* spriteCommon_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;

    // バッファリソース
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
    // バッファリソース内のデータを指すポインタ
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    Material* materialData_ = nullptr;
    TransformationMatrix* transformationMatrixData_ = nullptr;
    // バッファリソースの使い道を捕捉するバッファビュー
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_;
    D3D12_INDEX_BUFFER_VIEW indexBufferView_;

    MatrixMath::Vector2 pos_ = { 0.0f,0.0f };
    float rotation_ = 0.0f;
    MatrixMath::Vector2 size_ = { 1.0f,1.0f };

    // テクスチャ番号
    uint32_t textureIndex_ = 0;

    MatrixMath::Vector2 anchorPoint_ = { 0.0f,0.0f };

    // 左右フリップ
	bool isFlipX_ = false;
	// 上下フリップ
	bool isFlipY_ = false;

    // テクスチャ左上座標
    MatrixMath::Vector2 textureLeftTop_ = { 0.0f,0.0f };
    // テクスチャ切り出しサイズ
    MatrixMath::Vector2 textureSize_ = { 100.0f,100.0f };

    std::string filePath_;
    uint32_t srvIndex_ = 0;
};

