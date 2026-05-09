#include "2d/Sprite.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "base/SrvManager.h"
#include "base/WinApp.h"
#include "math/Transform.h"

void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath) {
	spriteCommon_ = spriteCommon;
	dxCommon_ = spriteCommon_->GetDxCommon();
	filePath_ = textureFilePath;
	srvIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);

	CreateVertexData();
	CreateIndexData();
	CreateMaterialData();
	CreateTransformationMatrixData();

	// 初期サイズ設定
	AdjustTextureSize();
}

void Sprite::Update() {
	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	if (isFlipX_) {
		std::swap(left, right);
	}
	if (isFlipY_) {
		std::swap(top, bottom);
	}

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);

	// UV計算
	float tex_left = textureLeftTop_.x / (float)metadata.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / (float)metadata.width;
	float tex_top = textureLeftTop_.y / (float)metadata.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / (float)metadata.height;

	// 頂点データ更新 (Zは0、Wは1)
	vertexData_[0] = {
	    {left, bottom, 0.0f, 1.0f},
        {tex_left, tex_bottom},
        {0, 0, -1}
    }; // 左下
	vertexData_[1] = {
	    {left, top, 0.0f, 1.0f},
        {tex_left, tex_top},
        {0, 0, -1}
    }; // 左上
	vertexData_[2] = {
	    {right, bottom, 0.0f, 1.0f},
        {tex_right, tex_bottom},
        {0, 0, -1}
    }; // 右下
	vertexData_[3] = {
	    {right, top, 0.0f, 1.0f},
        {tex_right, tex_top},
        {0, 0, -1}
    }; // 右上

	// 行列更新
	Transform transform{
	    {size_.x, size_.y, 1.0f     },
        {0.0f,    0.0f,    rotation_},
        {pos_.x,  pos_.y,  0.0f     }
    };
	Matrix4x4 worldMatrix = Matrix4x4::Affine(transform.scale, transform.rotate, transform.translate);

	// 2D用正射影行列 (左上0,0 右下 WinApp::kClientWidth, WinApp::kClientHeight)
	Matrix4x4 projectionMatrix = Matrix4x4::Orthographic(0.0f, 0.0f, (float)WinApp::kClientWidth, (float)WinApp::kClientHeight, 0.0f, 100.0f);

	transformationMatrixData_->WVP = worldMatrix * projectionMatrix;
	transformationMatrixData_->World = worldMatrix;
}

void Sprite::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	auto* srvManager = spriteCommon_->GetSrvManager();

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	commandList->IASetIndexBuffer(&indexBufferView_);
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	commandList->SetGraphicsRootDescriptorTable(2, srvManager->GetGPUDescriptorHandle(srvIndex_));

	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateVertexData() {
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 4);
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
	vertexBufferView_.StrideInBytes = sizeof(VertexData);
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
}

void Sprite::CreateIndexData() {
	indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * 6);
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2; // 三角形1
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2; // 三角形2
}

void Sprite::CreateMaterialData() {
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	materialData_->color = {1.0f, 1.0f, 1.0f, 1.0f};
	materialData_->uvTransform = Matrix4x4::Identity();
}

void Sprite::CreateTransformationMatrixData() {
	transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));
}

void Sprite::AdjustTextureSize() {
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);
	textureSize_ = {(float)metadata.width, (float)metadata.height};
	size_ = textureSize_;
}