#include "stage/Stage.h"

#include "stage/StageSerializer.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

bool Stage::LoadFromFile(const std::string& path) {
	if (!StageSerializer::Load(path, data_)) {
		return false;
	}
	Rebuild();
	return true;
}

void Stage::Rebuild() {
	// 作り直しなので既存の実体は全て破棄する
	objects_.clear();
	objects_.reserve(data_.objects.size());

	for (const StageData::ObjectData& objectData : data_.objects) {
		// 無効フラグ付きはデータとして保持しつつ実体を作らない(indexはdata_.objectsと揃える)
		if (objectData.disabled) {
			objects_.push_back(nullptr);
			continue;
		}

		// 読み込み済みなら早期returnするので、同じモデルを複数オブジェクトで共有できる
		ModelManager::GetInstance()->LoadModel(objectData.model);

		std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
		object->Initialize(Object3dCommon::GetInstance());
		object->SetModel(objectData.model);
		object->GetTransform() = objectData.transform;
		if (camera_) {
			object->SetCamera(camera_);
		}
		objects_.push_back(std::move(object));
	}
}

void Stage::Update(float deltaTime) {
	for (const std::unique_ptr<Object3d>& object : objects_) {
		if (object) {
			object->Update(deltaTime);
		}
	}
}

void Stage::Draw() {
	for (const std::unique_ptr<Object3d>& object : objects_) {
		if (object) {
			object->Draw();
		}
	}
}

void Stage::SetCamera(Camera* camera) {
	camera_ = camera;
	for (const std::unique_ptr<Object3d>& object : objects_) {
		if (object) {
			object->SetCamera(camera);
		}
	}
}

// Object3dの完全型が必要なため、デストラクタは.cpp側に置く(ヘッダは前方宣言のみ)
Stage::Stage() = default;

Stage::~Stage() = default;
