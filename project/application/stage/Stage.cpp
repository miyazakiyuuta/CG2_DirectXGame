#include "stage/Stage.h"

#include "stage/StageSerializer.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

#include <cmath>

bool Stage::LoadFromFile(const std::string& path) {
	if (!StageSerializer::Load(path, data_)) {
		return false;
	}
	Rebuild();
	return true;
}

void Stage::Rebuild() {
	// 既存の実体は即deleteせず墓場へ退避する。ImGuiのボタン処理等、描画コマンド記録後に
	// Rebuildされた場合、このフレームのコマンドリストが古い実体の定数バッファを参照済みのため、
	// ここでdeleteするとOBJECT_DELETED_WHILE_STILL_IN_USEでGPU実行が壊れる
	for (std::unique_ptr<Object3d>& object : objects_) {
		if (object) {
			graveyard_.push_back(std::move(object));
		}
	}
	objects_.clear();
	objects_.reserve(data_.objects.size());

	for (const StageData::ObjectData& objectData : data_.objects) {
		// 無効フラグ付きはデータとして保持しつつ実体を作らない(indexはdata_.objectsと揃える)。
		// 敵の発生地点(Spawn)もここでは実体を持たない(生成の責任はEnemySpawner側)
		if (objectData.disabled || objectData.type == StageData::ObjectType::Spawn) {
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

	// レール曲線も「データ→実体」の一部としてここで再構築する。
	// Reload(ファイル読み+Rebuild)や将来のPlay in Editor(Capture+Rebuild)でも自動で追従する
	rail_.SetControlPoints(data_.rail);

	// 当たり判定もデータから導かれる実体の一部。Rebuild直後のフレームで
	// 判定が1フレーム空にならないよう、ここでも作っておく
	UpdateWorldColliders();
}

void Stage::UpdateWorldColliders() {
	worldColliders_.clear();

	for (size_t i = 0; i < data_.objects.size(); ++i) {
		const StageData::ObjectData& objectData = data_.objects[i];
		// コライダー未設定と、無効フラグで実体を持たないものは判定対象外
		// (見えていないものに当たると理不尽なため、描画と判定の有無を一致させる)
		if (!objectData.hasCollider || objectData.disabled) {
			continue;
		}

		// AABBは軸平行なので回転を表現できない。translateとscaleのみ適用し、
		// rotateは意図的に無視する(回転が必要になったらOBBを別途用意すること)
		const Transform& transform = objectData.transform;
		const Vector3 center = {
			transform.translate.x + objectData.collider.center.x * transform.scale.x,
			transform.translate.y + objectData.collider.center.y * transform.scale.y,
			transform.translate.z + objectData.collider.center.z * transform.scale.z,
		};
		// sizeは「各軸の大きさ」なので、中心から広げる量は半分
		const Vector3 halfSize = {
			std::abs(objectData.collider.size.x * transform.scale.x) * 0.5f,
			std::abs(objectData.collider.size.y * transform.scale.y) * 0.5f,
			std::abs(objectData.collider.size.z * transform.scale.z) * 0.5f,
		};

		AABB aabb;
		aabb.min = center - halfSize;
		aabb.max = center + halfSize;
		worldColliders_.push_back(aabb);
	}
}

void Stage::Update(float deltaTime) {
	// 前フレームのコマンドはPostDrawのフェンス待ちで実行完了済みなので、退避分をここで破棄する
	graveyard_.clear();

	for (size_t i = 0; i < objects_.size(); ++i) {
		if (objects_[i]) {
			// StageDataが唯一の編集対象(Source of Truth)。エディタでの変更を毎フレーム実体へ反映する
			objects_[i]->GetTransform() = data_.objects[i].transform;
			objects_[i]->Update(deltaTime);
		}
	}

	// ギズモやInspectorでの編集を当たり判定にも即座に反映する
	// (Save/Reloadを挟まずその場で確認できる)
	UpdateWorldColliders();
}

void Stage::Draw() {
	for (const std::unique_ptr<Object3d>& object : objects_) {
		if (object) {
			object->Draw();
		}
	}
}

size_t Stage::AddObject(StageData::ObjectData objectData) {
	objectData.name = MakeUniqueName(objectData.name);
	data_.objects.push_back(std::move(objectData));
	Rebuild();
	return data_.objects.size() - 1;
}

size_t Stage::DuplicateObject(size_t index) {
	StageData::ObjectData copied = data_.objects[index];
	copied.name = MakeUniqueName(copied.name);
	// Hierarchy上で元の隣に並ぶよう直後へ挿入する
	data_.objects.insert(data_.objects.begin() + index + 1, std::move(copied));
	Rebuild();
	return index + 1;
}

void Stage::RemoveObject(size_t index) {
	data_.objects.erase(data_.objects.begin() + index);
	Rebuild();
}

void Stage::SetObjectModel(size_t index, const std::string& model) {
	data_.objects[index].model = model;
	// 新モデルのロードはRebuild内のLoadModelが、古い実体の安全な破棄はgraveyard機構が行う
	Rebuild();
}

void Stage::SetObjectDisabled(size_t index, bool disabled) {
	if (data_.objects[index].disabled == disabled) {
		return;
	}
	data_.objects[index].disabled = disabled;
	// 実体の生成/破棄はRebuildに集約する(有効化時のモデルロードや遅延削除も既存経路に乗る)
	Rebuild();
}

std::string Stage::MakeUniqueName(const std::string& base) const {
	auto exists = [this](const std::string& name) {
		for (const StageData::ObjectData& objectData : data_.objects) {
			if (objectData.name == name) {
				return true;
			}
		}
		return false;
	};

	if (!exists(base)) {
		return base;
	}
	// 空き番号が見つかるまで加算する(オブジェクト数は有限なので必ず終わる)
	for (int i = 1;; ++i) {
		std::string candidate = base + "_" + std::to_string(i);
		if (!exists(candidate)) {
			return candidate;
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
		editorObject.disabled = objectData.disabled;
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
