#include "StageEdit.h"

#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "3d/Camera.h"
#include "io/Input.h"
#include <imgui.h>
#include <algorithm>

void StageEdit::Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& blockModelName){
	object3dCommon_ = object3dCommon;
	camera_ = camera;
	blockModelName_ = blockModelName;
}

void StageEdit::Update(){
	Input* input = Input::GetInstance();

	if(input->IsTriggerKey(DIK_F1)){
		isEditMode_ = !isEditMode_;
	}

	if(!isEditMode_){
		return;
	}

	DrawImGui();
}

void StageEdit::Draw(){
	for(auto& block : blocks_){
		if(block.object){
			block.object->Update();
			block.object->Draw();
		}
	}
}

void StageEdit::Clear(){
	blocks_.clear();
}

void StageEdit::CreateSingleBlock(const Vector3& position){
	Block block{};

	block.object = std::make_unique<Object3d>();
	block.object->Initialize(object3dCommon_);
	block.object->SetModel(blockModelName_);
	block.object->SetCamera(camera_);
	block.object->SetTranslate(position);
	block.object->SetRotate({ 0.0f, 0.0f, 0.0f });
	block.object->Update();

	block.position = position;

	blocks_.push_back(std::move(block));
}

void StageEdit::CreateBlockRange(const Vector3& minPos, const Vector3& maxPos, const Vector3& spacing){
	if(spacing.x <= 0.0f || spacing.y <= 0.0f || spacing.z <= 0.0f){
		return;
	}

	Vector3 fixedMin{
		std::min(minPos.x, maxPos.x),
		std::min(minPos.y, maxPos.y),
		std::min(minPos.z, maxPos.z)
	};

	Vector3 fixedMax{
		std::max(minPos.x, maxPos.x),
		std::max(minPos.y, maxPos.y),
		std::max(minPos.z, maxPos.z)
	};

	for(float y = fixedMin.y; y <= fixedMax.y + 0.001f; y += spacing.y){
		for(float z = fixedMin.z; z <= fixedMax.z + 0.001f; z += spacing.z){
			for(float x = fixedMin.x; x <= fixedMax.x + 0.001f; x += spacing.x){
				CreateSingleBlock({ x, y, z });
			}
		}
	}
}

std::vector<CollisionUtility::AABB> StageEdit::GetBlockAABBs() const{
	std::vector<CollisionUtility::AABB> result;
	result.reserve(blocks_.size());

	for(const auto& block : blocks_){
		CollisionUtility::AABB aabb;
		aabb.min = {
			block.position.x - blockHalfSize_.x,
			block.position.y - blockHalfSize_.y,
			block.position.z - blockHalfSize_.z
		};
		aabb.max = {
			block.position.x + blockHalfSize_.x,
			block.position.y + blockHalfSize_.y,
			block.position.z + blockHalfSize_.z
		};
		result.push_back(aabb);
	}

	return result;
}

void StageEdit::DrawImGui(){
	ImGui::Begin("StageEdit");

	ImGui::Text("F1 : Toggle StageEdit");
	ImGui::Separator();
	ImGui::Text("Block Count : %d", static_cast<int>(blocks_.size()));

	ImGui::Separator();
	ImGui::Text("Create Range Blocks");

	ImGui::DragFloat3("Range Min", &rangeMin_.x, 0.1f);
	ImGui::DragFloat3("Range Max", &rangeMax_.x, 0.1f);
	ImGui::DragFloat3("Spacing", &spacing_.x, 0.1f, 0.1f, 100.0f);
	ImGui::DragFloat("Y Offset", &yOffset_, 0.1f);

	Vector3 previewMin = rangeMin_;
	Vector3 previewMax = rangeMax_;
	previewMin.y += yOffset_;
	previewMax.y += yOffset_;

	ImGui::Text("Preview Min : (%.2f, %.2f, %.2f)", previewMin.x, previewMin.y, previewMin.z);
	ImGui::Text("Preview Max : (%.2f, %.2f, %.2f)", previewMax.x, previewMax.y, previewMax.z);

	if(ImGui::Button("Create Range Blocks")){
		CreateBlockRange(previewMin, previewMax, spacing_);
	}

	if(ImGui::Button("Clear All Blocks")){
		Clear();
	}

	ImGui::End();
}