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
	for (size_t i = 0; i < objects_.size(); ++i) {
		if (objects_[i]) {
			// StageDataが唯一の編集対象(Source of Truth)。エディタでの変更を毎フレーム実体へ反映する
			objects_[i]->GetTransform() = data_.objects[i].transform;
			objects_[i]->Update(deltaTime);
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

std::vector<EditorObject> Stage::BuildEditorObjects() {
	std::vector<EditorObject> editorObjects;
	editorObjects.reserve(data_.objects.size());
	for (StageData::ObjectData& objectData : data_.objects) {
		EditorObject editorObject;
		editorObject.name = objectData.name;
		editorObject.transform = &objectData.transform;
		// disabledは実体が無く画面に見えないため、クリック選択の対象外(Hierarchyからは選べる)
		editorObject.pickable = !objectData.disabled;
		editorObjects.push_back(std::move(editorObject));
	}
	return editorObjects;
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
