#include "enemy/EnemySpawner.h"

#include "stage/StageData.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"

namespace {
	// 敵の種別 → モデルの対応表。データ(stage.json)にはモデルパスを持たせず種別だけを持たせ、
	// 「どの見た目で出すか」はコード側のこの1箇所で決める(種別追加はここに足すだけ)
	const std::string kFrogModel = "frog/Frog.gltf";
	const std::string kSwordModel = "sword/sword.obj";
}

const std::string& EnemySpawner::ModelPathOf(const std::string& enemy) {
	if (enemy == "sword") {
		return kSwordModel;
	}
	return kFrogModel; // 既定(未知の種別もここへ落ちるので、データ不備でも消えずに気付ける)
}

float EnemySpawner::ScaleOf(const std::string& enemy) {
	// モデルごとに実寸が違うため、見た目の大きさを揃える倍率を種別に紐づける
	if (enemy == "sword") {
		return 2.0f; // sword: 約1.2 x 4.9 x 0.5m
	}
	return 3.0f;     // frog : 約1.8 x 1.9 x 2.2m
}

const std::vector<std::string>& EnemySpawner::EnemyTypes() {
	static const std::vector<std::string> kTypes = { "frog", "sword" };
	return kTypes;
}

void EnemySpawner::RetireEnemies() {
	for (std::unique_ptr<Object3d>& enemy : enemies_) {
		graveyard_.push_back(std::move(enemy));
	}
	enemies_.clear();
}

void EnemySpawner::BuildFromStage(const StageData& stageData) {
	// 構築し直すので、発生済みの敵も含めて作り直す(Reloadで配置が変わるため)
	spawnPoints_.clear();
	RetireEnemies();

	for (const StageData::ObjectData& objectData : stageData.objects) {
		if (objectData.type != StageData::ObjectType::Spawn) {
			continue;
		}
		// 無効フラグはstaticと同じ意味(データは残すがゲームには出さない)
		if (objectData.disabled) {
			continue;
		}

		SpawnPoint spawnPoint;
		spawnPoint.enemy = objectData.enemy;
		spawnPoint.railDistance = objectData.railDistance;
		spawnPoint.transform = objectData.transform;
		spawnPoints_.push_back(std::move(spawnPoint));

		// 発生時に読み込むと1フレーム重くなるため、モデルは先に読んでおく
		ModelManager::GetInstance()->LoadModel(ModelPathOf(objectData.enemy));
	}
}

void EnemySpawner::Update(float railDistance, float deltaTime) {
	// 前フレームのコマンドはPostDrawのフェンス待ちで実行完了済みなので、退避分をここで破棄する
	graveyard_.clear();

	for (SpawnPoint& spawnPoint : spawnPoints_) {
		// 到達した瞬間に1度だけ発生させる(Resetするまで再発生しない)
		if (spawnPoint.spawned || railDistance < spawnPoint.railDistance) {
			continue;
		}
		spawnPoint.spawned = true;

		std::unique_ptr<Object3d> enemy = std::make_unique<Object3d>();
		enemy->Initialize(Object3dCommon::GetInstance());
		enemy->SetModel(ModelPathOf(spawnPoint.enemy));
		// 出現位置はSpawnPointのtransform(エディタのギズモで置いた場所)。
		// 大きさだけは種別ごとの既定を使い、データ側のscaleは向き・位置に集中させる
		enemy->GetTransform() = spawnPoint.transform;
		const float scale = ScaleOf(spawnPoint.enemy);
		enemy->SetScale({ scale, scale, scale });
		if (camera_) {
			enemy->SetCamera(camera_);
		}
		enemies_.push_back(std::move(enemy));
	}

	for (std::unique_ptr<Object3d>& enemy : enemies_) {
		enemy->Update(deltaTime);
	}
}

void EnemySpawner::Reset() {
	// ImGuiのボタン(Respawn/Reset)から呼ばれるため、こちらも即deleteしない
	RetireEnemies();
	for (SpawnPoint& spawnPoint : spawnPoints_) {
		spawnPoint.spawned = false;
	}
}

void EnemySpawner::Draw() {
	for (const std::unique_ptr<Object3d>& enemy : enemies_) {
		enemy->Draw();
	}
}

void EnemySpawner::SetCamera(Camera* camera) {
	camera_ = camera;
	for (std::unique_ptr<Object3d>& enemy : enemies_) {
		enemy->SetCamera(camera);
	}
}

EnemySpawner::EnemySpawner() = default;

EnemySpawner::~EnemySpawner() = default;
