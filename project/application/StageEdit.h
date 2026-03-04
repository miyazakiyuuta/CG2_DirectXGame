#pragma once

#define NOMINMAX
#include <memory>
#include <string>
#include <vector>

#include "math/Vector3.h"

class Camera;
class Object3d;
class Object3dCommon;

class StageEdit{
public:
	StageEdit() = default;
	~StageEdit() = default;

	void Initialize(Object3dCommon* object3dCommon, Camera* camera, const std::string& blockModelName);

	void Update();
	void Draw();

	bool IsEditMode() const{ return isEditMode_; }

	// 範囲生成
	void CreateBlockRange(const Vector3& minPos, const Vector3& maxPos, const Vector3& spacing);

	// 全削除
	void Clear();

	// 必要なら外から参照
	size_t GetBlockCount() const{ return blocks_.size(); }

private:
	void DrawImGui();
	void CreateSingleBlock(const Vector3& position);

private:
	struct Block{
		std::unique_ptr<Object3d> object;
		Vector3 position{};
	};

	Object3dCommon* object3dCommon_ = nullptr;
	Camera* camera_ = nullptr;
	std::string blockModelName_;

	std::vector<Block> blocks_;

	bool isEditMode_ = false;

	// ImGui 用の範囲指定
	Vector3 rangeMin_ = { -2.0f, 0.0f, -2.0f };
	Vector3 rangeMax_ = { 2.0f, 0.0f,  2.0f };
	Vector3 spacing_ = { 2.0f, 2.0f,  2.0f };

	// まとめて少し上に置きたい時用
	float yOffset_ = 0.0f;
};