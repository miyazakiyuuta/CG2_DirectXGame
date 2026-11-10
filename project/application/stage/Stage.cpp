#include "stage/Stage.h"

#include "stage/StageSerializer.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

#include <algorithm>
#include <cmath>
#include <format>

namespace {
	// 曲線(Catmull-Rom)を作るのに要る制御点の最小数
	constexpr size_t kMinRailPoints = 2;
	// 制御点をクリックで選ぶときの判定球の半径。表示の球(GamePlaySceneでShow Rail時に描く)より大きめにして掴みやすくする
	constexpr float kRailPointPickRadius = 1.5f;

	// レール上の場所を「何番目の区間(制御点 i と i+1 の間)の、長さの何割か」で表したもの
	struct RailAnchor {
		size_t segment = 0;
		float fraction = 0.0f; // 0=区間の始まり 〜 1=終わり
	};

	// 距離 → 区間番号と割合(railはまだ形を変える前の曲線)
	RailAnchor ToAnchor(const CatmullRomSpline& rail, float distance) {
		const size_t count = rail.GetSegmentCount();
		if (count == 0) {
			return {};
		}
		// 区間の数は制御点の数-1(高々数十)なので先頭から順に探せば足りる
		for (size_t i = 0; i < count; ++i) {
			const float start = rail.GetSegmentStartDistance(i);
			const float length = rail.GetSegmentLength(i);
			if (distance < start + length || i + 1 == count) {
				const float fraction = length > 0.0f ? std::clamp((distance - start) / length, 0.0f, 1.0f) : 0.0f;
				return { i, fraction };
			}
		}
		return {};
	}

	// 区間番号と割合 → 距離(railは形を変えた後の曲線)
	float ToDistance(const CatmullRomSpline& rail, const RailAnchor& anchor) {
		if (rail.GetSegmentCount() == 0) {
			return 0.0f;
		}
		return rail.GetSegmentStartDistance(anchor.segment) + rail.GetSegmentLength(anchor.segment) * anchor.fraction;
	}

	// 2つの制御点列が完全に同じか(Vector3に比較演算子が無いので成分で比べる)
	bool SamePoints(const std::vector<Vector3>& a, const std::vector<Vector3>& b) {
		return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](const Vector3& p, const Vector3& q) {
			return p.x == q.x && p.y == q.y && p.z == q.z;
		});
	}
}

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
	// Reload(ファイル読み+Rebuild)や将来のPlay in Editor(Capture+Rebuild)でも自動で追従する。
	// ここはデータをそのまま形にするだけなので、距離の付け直しはしない(付け直すのはエディタで形を変えたときだけ)
	rail_.SetControlPoints(data_.rail);
	++railRevision_;

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

	// 制御点がギズモやInspectorで動いていたら、レールを作り直す(配置オブジェクトの編集を実体へ写すのと同じ場所)。
	// 制御点の数は変わっていないので、区間番号はそのまま(どの区間の何割か、を保って距離だけ付け直す)
	if (!SamePoints(data_.rail, rail_.GetControlPoints())) {
		RebuildRailKeepingDistances([](RailAnchor anchor) { return anchor; });
	}
}

template <class RemapSegment>
void Stage::RebuildRailKeepingDistances(RemapSegment remapSegment) {
	// レールに沿った距離で書かれたデータ。区間の開始距離と、敵の出現距離(無効フラグ付きの出現地点も含む。
	// 無効はデータとして残す意味なので、有効に戻したとき正しい場所を指しているように)
	std::vector<float*> distances;
	for (StageData::SectionData& section : data_.sections) {
		distances.push_back(&section.startDistance);
	}
	for (StageData::ObjectData& objectData : data_.objects) {
		if (objectData.type == StageData::ObjectType::Spawn) {
			distances.push_back(&objectData.railDistance);
		}
	}

	// ① 古い形(rail_はまだ作り直していない)で、区間番号と割合に置き換える。② 番号を付け替える
	std::vector<RailAnchor> anchors;
	anchors.reserve(distances.size());
	for (const float* distance : distances) {
		anchors.push_back(remapSegment(ToAnchor(rail_, *distance)));
	}

	// ③ 新しい制御点で作り直し、同じ場所を新しい距離へ戻す
	rail_.SetControlPoints(data_.rail);
	for (size_t i = 0; i < distances.size(); ++i) {
		*distances[i] = ToDistance(rail_, anchors[i]);
	}
	// 先頭の区間は必ずレールの始点(0)から(StageData::sectionsの約束)。付け直しの丸め誤差で0からずれないようにする
	if (!data_.sections.empty()) {
		data_.sections.front().startDistance = 0.0f;
	}
	++railRevision_;
}

std::vector<EditorObject> Stage::BuildRailEditorObjects() {
	std::vector<EditorObject> editorObjects;
	editorObjects.reserve(data_.rail.size());
	for (size_t i = 0; i < data_.rail.size(); ++i) {
		EditorObject editorObject;
		editorObject.name = std::format("rail_{:02}", i);
		editorObject.position = &data_.rail[i];
		editorObject.pickRadius = kRailPointPickRadius;
		editorObjects.push_back(std::move(editorObject));
	}
	return editorObjects;
}

size_t Stage::InsertRailPoint(size_t index) {
	std::vector<Vector3>& points = data_.rail;
	if (index >= points.size()) {
		return index; // 範囲外(呼び出し側の選択がずれている)。何もしない
	}

	Vector3 newPoint{};
	if (index + 1 < points.size() && index < rail_.GetSegmentCount()) {
		// 次の点との間: 今の曲線上で、区間の長さのちょうど半分の位置
		const float middle = rail_.GetSegmentStartDistance(index) + rail_.GetSegmentLength(index) * 0.5f;
		newPoint = rail_.GetPositionByDistance(middle);
	} else if (index > 0) {
		// 最後の点の後ろ: 直前の区間と同じ向き・長さだけ先へ延ばす
		newPoint = points[index] + (points[index] - points[index - 1]);
	} else {
		// 制御点が1つしかない(レールになっていない)。前方(+Z)へ少し離して置く
		newPoint = points[index] + Vector3{ 0.0f, 0.0f, 10.0f };
	}
	points.insert(points.begin() + static_cast<std::ptrdiff_t>(index) + 1, newPoint);

	// 区間番号の付け替え: index番の区間が中間で2つに分かれる(前半→index、後半→index+1)。
	// それより後ろの区間は1つずつ後ろへずれ、手前はそのまま。
	// 最後の点の後ろに足した場合は、既存の区間はすべてindexより手前なので何も変わらない
	RebuildRailKeepingDistances([index](RailAnchor anchor) -> RailAnchor {
		if (anchor.segment < index) {
			return anchor;
		}
		if (anchor.segment > index) {
			return { anchor.segment + 1, anchor.fraction };
		}
		// 中間(割合0.5)で分かれるので、前半・後半それぞれの中での割合へ引き伸ばす
		return anchor.fraction < 0.5f
			? RailAnchor{ index, anchor.fraction * 2.0f }
			: RailAnchor{ index + 1, (anchor.fraction - 0.5f) * 2.0f };
	});
	return index + 1;
}

void Stage::RemoveRailPoint(size_t index) {
	std::vector<Vector3>& points = data_.rail;
	if (index >= points.size() || points.size() <= kMinRailPoints) {
		return; // 範囲外か、消すと曲線が作れなくなる
	}

	// 消す前の区間の長さ(真ん中の点を消すと、両側の2区間が1つにまとまる。その中での割合を長さで按分する)
	const size_t lastIndex = points.size() - 1;
	const float previousLength = index > 0 ? rail_.GetSegmentLength(index - 1) : 0.0f;
	const float nextLength = index < lastIndex ? rail_.GetSegmentLength(index) : 0.0f;
	points.erase(points.begin() + static_cast<std::ptrdiff_t>(index));

	RebuildRailKeepingDistances([index, lastIndex, previousLength, nextLength](RailAnchor anchor) -> RailAnchor {
		if (index == 0) {
			// 先頭の点を消した: 最初の区間がなくなる。そこにあった場所は新しい始点へ寄せる
			return anchor.segment == 0 ? RailAnchor{ 0, 0.0f } : RailAnchor{ anchor.segment - 1, anchor.fraction };
		}
		if (index == lastIndex) {
			// 最後の点を消した: 最後の区間がなくなる。そこにあった場所は新しい終点へ寄せる
			return anchor.segment + 1 == lastIndex ? RailAnchor{ lastIndex - 2, 1.0f } : anchor;
		}
		// 真ん中の点を消した: 区間 index-1 と index が1つ(index-1)にまとまる
		const float merged = previousLength + nextLength;
		if (anchor.segment + 1 < index) {
			return anchor;
		}
		if (anchor.segment + 1 == index) {
			return { index - 1, merged > 0.0f ? anchor.fraction * previousLength / merged : 0.0f };
		}
		if (anchor.segment == index) {
			return { index - 1, merged > 0.0f ? (previousLength + anchor.fraction * nextLength) / merged : 1.0f };
		}
		return { anchor.segment - 1, anchor.fraction };
	});
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
