#include "Camera.h"
#include "WinApp.h"
#include <cassert>


#ifdef _DEBUG
#include "externals/imgui/imgui.h"
#endif // _DEBUG


using namespace MatrixMath;

Camera::Camera()
	: transform_({ 1.0f,1.0f,1.0f }, { 0.0f,0.0f,0.0f }, { 0.0f,0.0f,0.0f })
	, fovY_(0.45f)
	, aspectRatio_(float(WinApp::kClientWidth) / float(WinApp::kClientHeight))
	, nearClip_(0.1f)
	, farClip_(100.0f)
	, worldMatrix_(MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate))
	, viewMatrix_(Inverse(worldMatrix_))
	, projectionMatrix_(MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_))
	, viewProjectionMatrix_(Multiply(viewMatrix_, projectionMatrix_)) {
}

void Camera::DrawImGui() {
#ifdef _DEBUG
	if (ImGui::TreeNode("Camera")) {
		ImGui::DragFloat3("Camera Position", &transform_.translate.x, 0.01f);
		ImGui::DragFloat3("Camera Rotation", &transform_.rotate.x, 0.01f);
		ImGui::TreePop();
	}
#endif
}

void Camera::InitializeGPU(ID3D12Device* device) {
	// 定数バッファのサイズは 256byte アラインが安全（D3D12のCB要件）
	const UINT sizeInBytes = (sizeof(CameraForGPU) + 0xFF) & ~0xFF;

	// UploadHeap で作る
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	HRESULT hr = device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&cameraResource_)
	);
	assert(SUCCEEDED(hr));

	// Mapして保持（Unmapしない方式）
	hr = cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&mappedCameraData_));
	assert(SUCCEEDED(hr));

	// 初期値反映
	TransferToGPU();
}

void Camera::Update() {
	// Transformからアフィン変換行列を計算
	worldMatrix_ = MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);
	// worldMatrixの逆行列
	viewMatrix_ = Inverse(worldMatrix_);
	// 透視投影行列を計算
	projectionMatrix_ = MakePerspectiveFovMatrix(fovY_, aspectRatio_, nearClip_, farClip_);
	// ビュー行列とプロジェクション行列の積を計算
	viewProjectionMatrix_ = Multiply(viewMatrix_, projectionMatrix_);
}

void Camera::TransferToGPU() {
	if (!mappedCameraData_) { return; }

	mappedCameraData_->worldPosition = transform_.translate;
	mappedCameraData_->pad = 0.0f;
}

D3D12_GPU_VIRTUAL_ADDRESS Camera::GetGPUAddress() const {
	return cameraResource_ ? cameraResource_->GetGPUVirtualAddress() : 0;
}
