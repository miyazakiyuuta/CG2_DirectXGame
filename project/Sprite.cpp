#include "Sprite.h"
#include "SpriteCommon.h"
#include "WinApp.h"
#include "TextureManager.h"
#include "SrvManager.h"

using namespace MatrixMath;

void Sprite::Initialize(SpriteCommon* spriteCommon, std::string textureFilePath) {
	spriteCommon_ = spriteCommon;
	dxCommon_ = spriteCommon_->GetDxCommon();
	filePath_ = textureFilePath;
	textureIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);
	srvIndex_ = TextureManager::GetInstance()->GetSrvIndex(filePath_);

	CreateVertexData();

	CreateIndexData();

	CreateMaterialData();

	CreateTransformationMatrixData();

}

void Sprite::Update() {

	float left = 0.0f - anchorPoint_.x;
	float right = 1.0f - anchorPoint_.x;
	float top = 0.0f - anchorPoint_.y;
	float bottom = 1.0f - anchorPoint_.y;

	// 左右反転
	if (isFlipX_) {
		left = -left;
		right = -right;
	}
	// 上下反転
	if (isFlipY_) {
		top = -top;
		bottom = -bottom;
	}

	//const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex_);
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);
	float tex_left = textureLeftTop_.x / metadata.width;
	float tex_right = (textureLeftTop_.x + textureSize_.x) / metadata.width;
	float tex_top = textureLeftTop_.y / metadata.height;
	float tex_bottom = (textureLeftTop_.y + textureSize_.y) / metadata.height;

	// 頂点リソースにデータを書き込む(4点分)
	// 1枚目の三角形
	vertexData_[0].position = { left,bottom,0.0f,1.0f }; // 左下
	vertexData_[0].texcoord = { tex_left,tex_bottom };
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[1].position = { left,top,0.0f,1.0f }; // 左上
	vertexData_[1].texcoord = { tex_left,tex_top };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };
	vertexData_[2].position = { right,bottom,0.0f,1.0f }; // 右下
	vertexData_[2].texcoord = { tex_right,tex_bottom };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };
	// 2枚目の三角形
	vertexData_[3].position = { right,top,0.0f,1.0f }; // 左上
	vertexData_[3].texcoord = { tex_right,tex_top };
	vertexData_[3].normal = { 0.0f,0.0f,1.0f };
	vertexData_[4].position = { right,top,0.0f,1.0f }; // 右上
	vertexData_[4].texcoord = { tex_right,tex_top };
	vertexData_[4].normal = { 0.0f,0.0f,1.0f };
	vertexData_[5].position = { right,bottom,0.0f,1.0f }; // 右下
	vertexData_[5].texcoord = { tex_right,tex_top };
	vertexData_[5].normal = { 0.0f,0.0f,1.0f };

	// インデックスリソースにデータを書き込む(6個分)
	// IndexResourceにデータを書き込むためのアドレスを取得してindexDataに割り当てる
	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;

	// Transform情報を作る
	Transform transform{ {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };
	transform.translate = { pos_.x,pos_.y,0.0f };
	transform.rotate = { 0.0f,0.0f,rotation_ };
	transform.scale = { size_.x,size_.y,1.0f };

	// TransformからWorldMatrixを作る
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	// ViewMatrixを作って単位行列を代入
	Matrix4x4 viewMatrix = MakeIdentity4x4();
	// ProjectionMatrixを使って平行投影行列を書き込む
	Matrix4x4 projectionMatrix = MakeOrthographicMatrix(0.0f, 0.0f, float(WinApp::kClientWidth), float(WinApp::kClientHeight), 0.0f, 100.0f);

	transformationMatrixData_->WVP = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	transformationMatrixData_->World = worldMatrix;
}

void Sprite::Draw() {
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
	auto* srvManager = spriteCommon_->GetSrvManager();

	// VertexBufferViewを設定
	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
	// IndexBufferViewを設定
	commandList->IASetIndexBuffer(&indexBufferView_);

	// マテリアルCBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());
	// 座標変換行列CBufferの場所を設定
	commandList->SetGraphicsRootConstantBufferView(1, transformationMatrixResource_->GetGPUVirtualAddress());
	// SRVのDescriptorTableの先頭を設定
	//D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = dxCommon_->GetSRVGPUDescriptorHandle(1);
	commandList->SetGraphicsRootDescriptorTable(
		2, // SRV用のルートパラメータ番号
		srvManager->GetGPUDescriptorHandle(srvIndex_)
	);
	// 描画！(DrawCall/ドローコール)
	commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateVertexData() {
	// VertexResourceを作る
	vertexResource_ = dxCommon_->CreateBufferResource(sizeof(VertexData) * 6);

	// VertexBufferViewを作成する
	// リソースの先頭のアドレスから使う
	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferView_.SizeInBytes = sizeof(VertexData) * 6;
	// 1頂点あたりのサイズ
	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	// VertexResourceにデータを書き込むためのアドレスを取得してvertexDataに割り当てる
	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

}

void Sprite::CreateIndexData() {
	// IndexResourceを作る
	indexResource_ = dxCommon_->CreateBufferResource(sizeof(uint32_t) * 6);

	// IndexBufferViewを作成する
	// リソースの先頭のアドレスから使う
	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

}

void Sprite::CreateMaterialData() {
	// マテリアルリソースを作る
	materialResource_ = dxCommon_->CreateBufferResource(sizeof(Material));

	// マテリアルリソースにデータを書き込むためのアドレスを取得してmaterialDataに割り当てる
	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));
	// マテリアルデータの初期値を書き込む
	materialData_->color = Vector4{ 1.0f,1.0f,1.0f,1.0f };
	materialData_->enableLighting = false;
	materialData_->uvTransform = MakeIdentity4x4();
}

void Sprite::CreateTransformationMatrixData() {
	// 座標変換行列リソースを作る
	transformationMatrixResource_ = dxCommon_->CreateBufferResource(sizeof(TransformationMatrix));

	// 座標変換行列リソースにデータを書き込むためのアドレスを取得してtransformationMatrixDataに割り当てる
	transformationMatrixResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	// 単位行列に書き込んでおく
	transformationMatrixData_->WVP = MakeIdentity4x4();
	transformationMatrixData_->World = MakeIdentity4x4();
	transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
}

void Sprite::AdjustTextureSize() {
	// テクスチャメタデータを取得
	//const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetadata(textureIndex_);
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance()->GetMetaData(filePath_);

	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);
	// 画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}
