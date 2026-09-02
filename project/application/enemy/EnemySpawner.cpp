#include "enemy/EnemySpawner.h"

#include "bullet/BulletManager.h"
#include "stage/StageData.h"
#include "math/Collision.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "audio/SoundManager.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 敵の種別 → モデルの対応表。データ(stage.json)にはモデルパスを持たせず種別だけを持たせ、
	// 「どの見た目で出すか」はコード側のこの1箇所で決める(種別追加はここに足すだけ)
	const std::string kFrogModel = "frog/Frog.gltf";
	const std::string kSwordModel = "sword/sword.obj";

	// 撃破エフェクトのグループ名。configとテクスチャの登録はシーン側で行う
	const std::string kDefeatEffect = "enemyExplosion";

	// 撃破音。発射音と違い同時に何発も鳴らないので、こちらは絞らない
	const std::string kDefeatSound = "resources/sounds/explosion.mp3";
	constexpr float kDefeatVolume = 0.8f;
}

const std::string& EnemySpawner::DefeatEffectName() {
	return kDefeatEffect;
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
	for (Enemy& enemy : enemies_) {
		graveyard_.push_back(std::move(enemy.object));
	}
	enemies_.clear();
	playerHit_ = false; // 敵が居なくなるので接触状態も落とす
}

void EnemySpawner::ApplyHpColor(Enemy& enemy) const {
	// HP満タンで白、減るほど赤へ寄せる。残量が常に見えるのでタイマー付きの点滅が要らない
	const float ratio = maxHp_ > 0
		? static_cast<float>(enemy.hp) / static_cast<float>(maxHp_)
		: 1.0f;
	const float rest = 0.2f + 0.8f * ratio; // HP0付近で0.2、満タンで1.0
	enemy.object->SetColor({ 1.0f, rest, rest, 1.0f });
}

void EnemySpawner::BuildFromStage(const StageData& stageData) {
	// 撃破音は初回だけ読む(BuildFromStageはReloadのたびに呼ばれるため)
	if (!defeatSound_) {
		defeatSound_ = SoundManager::GetInstance()->LoadFile(kDefeatSound);
	}

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

void EnemySpawner::UpdateBulletCollision() {
	if (!bulletManager_) {
		return; // 弾の供給元が無ければ被弾しない(エディタ単体での確認時など)
	}

	const std::vector<Bullet>& bullets = bulletManager_->GetBullets();
	const float bulletRadius = bulletManager_->GetRadius();

	// 総当たり。敵は同時数体、弾は最大64発なので空間分割は要らない
	for (size_t bulletIndex = 0; bulletIndex < bullets.size(); ++bulletIndex) {
		const Bullet& bullet = bullets[bulletIndex];
		if (!bullet.active) {
			continue;
		}

		for (Enemy& enemy : enemies_) {
			if (enemy.hp <= 0) {
				continue; // 同フレームに撃破済み。1発で2体分減らさない
			}
			if (!IsCollision(bullet.position, bulletRadius, enemy.position, enemy.radius)) {
				continue;
			}

			// 当たった弾は消す(貫通させない)。撃破処理はUpdate側でまとめて行う
			bulletManager_->Kill(bulletIndex);
			--enemy.hp;
			if (enemy.hp > 0) {
				ApplyHpColor(enemy); // 残りHPを色へ反映(撃破時は消えるので不要)
			}
			break; // この弾は消費済みなので次の弾へ
		}
	}
}

void EnemySpawner::UpdatePlayerCollision(const Vector3& playerPosition, float playerRadius) {
	// 1体でも触れていれば同じ結果(ダメージは無敵時間で1回にまとめられる)なので、
	// 見つかった時点で打ち切る
	playerHit_ = false;
	for (const Enemy& enemy : enemies_) {
		if (IsCollision(playerPosition, playerRadius, enemy.position, enemy.radius)) {
			playerHit_ = true;
			return;
		}
	}
}

void EnemySpawner::Update(float railDistance, float deltaTime, const Vector3& playerPosition, float playerRadius) {
	// 前フレームのコマンドはPostDrawのフェンス待ちで実行完了済みなので、退避分をここで破棄する
	graveyard_.clear();

	for (SpawnPoint& spawnPoint : spawnPoints_) {
		// 到達した瞬間に1度だけ発生させる(Resetするまで再発生しない)
		if (spawnPoint.spawned || railDistance < spawnPoint.railDistance) {
			continue;
		}
		spawnPoint.spawned = true;

		Enemy enemy;
		enemy.object = std::make_unique<Object3d>();
		enemy.object->Initialize(Object3dCommon::GetInstance());
		enemy.object->SetModel(ModelPathOf(spawnPoint.enemy));
		// 出現位置はSpawnPointのtransform(エディタのギズモで置いた場所)。
		// 大きさだけは種別ごとの既定を使い、データ側のscaleは向き・位置に集中させる
		enemy.object->GetTransform() = spawnPoint.transform;
		const float scale = ScaleOf(spawnPoint.enemy);
		enemy.object->SetScale({ scale, scale, scale });
		if (camera_) {
			enemy.object->SetCamera(camera_);
		}

		enemy.position = spawnPoint.transform.translate;
		enemy.radius = radius_;
		enemy.hp = maxHp_;
		ApplyHpColor(enemy);

		enemies_.push_back(std::move(enemy));
	}

	// 被弾判定は移動より先に行う。弾は既にこのフレーム分進んだ後なので、
	// 「弾が到達した位置」と「敵の現在位置」で判定でき、1フレームのズレが出ない
	UpdateBulletCollision();

	// 撃破された敵を取り除く(実体は墓場へ回して次フレーム冒頭で破棄する)
	for (size_t i = enemies_.size(); i > 0; --i) {
		Enemy& enemy = enemies_[i - 1];
		if (enemy.hp > 0) {
			continue;
		}
		// 撃破演出。敵の実体が消える前にその位置で発生させる。
		// パーティクルはParticleManagerが所有して寿命まで動かすので、
		// ここで敵を破棄しても演出は残る(発生源の生存に依存しない)
		ParticleManager::GetInstance()->Emit(
			kDefeatEffect, enemy.position, static_cast<uint32_t>(defeatParticleCount_));

		if (defeatSound_) {
			const SoundManager::SoundHandle handle = SoundManager::GetInstance()->PlayWave(
				defeatSound_, false, SoundManager::SoundCategory::SE);
			SoundManager::GetInstance()->SetVolume(handle, kDefeatVolume);
		}

		graveyard_.push_back(std::move(enemy.object));
		enemies_.erase(enemies_.begin() + static_cast<ptrdiff_t>(i - 1));
		++defeatedCount_;
	}

	// 自機へ近づく。ただしkeepDistance_より内側には入らない
	// (近づき続けると自機を追い越して背後へ消え、撃つ機会が無くなるため)
	for (Enemy& enemy : enemies_) {
		const Vector3 toPlayer = playerPosition - enemy.position;
		const float distance = std::sqrt(toPlayer.LengthSquared());
		if (distance > keepDistance_ && distance > 1.0e-4f) {
			// 行き過ぎて反転しないよう、残り距離を超えて進まないようにする
			const float step = (std::min)(moveSpeed_ * deltaTime, distance - keepDistance_);
			enemy.position += Vector3::Normalized(toPlayer) * step;
			enemy.object->SetTranslate(enemy.position);
		}
		enemy.object->Update(deltaTime);
	}

	// 自機との接触判定(A-5)。移動後の位置で行うので、寄ってきた敵がその場で当たる
	UpdatePlayerCollision(playerPosition, playerRadius);
}

void EnemySpawner::Reset() {
	// ImGuiのボタン(Respawn/Reset)から呼ばれるため、こちらも即deleteしない
	RetireEnemies();
	for (SpawnPoint& spawnPoint : spawnPoints_) {
		spawnPoint.spawned = false;
	}
}

void EnemySpawner::Draw() {
	for (const Enemy& enemy : enemies_) {
		enemy.object->Draw();
	}
}

void EnemySpawner::SetCamera(Camera* camera) {
	camera_ = camera;
	for (Enemy& enemy : enemies_) {
		enemy.object->SetCamera(camera);
	}
}

void EnemySpawner::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Enemy");
	ImGui::Text("Alive: %zu  Defeated: %d", enemies_.size(), defeatedCount_);
	// 体当たりが成立しているかの確認用(Keep Distanceを判定半径の和より大きくすると触れなくなる)
	if (playerHit_) {
		ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "Touching Player: YES");
	} else {
		ImGui::Text("Touching Player: no");
	}

	ImGui::SeparatorText("Parameters");
	// HPと判定半径の変更は次に発生する敵から効く(発生済みの個体は生成時の値を保持する)
	ImGui::DragInt("Max HP", &maxHp_, 0.1f, 1, 20);
	ImGui::DragFloat("Hit Radius", &radius_, 0.1f, 0.1f, 20.0f, "%.1f m");
	// 移動は全個体共通の値を毎フレーム参照するため、こちらは即座に効く
	ImGui::DragFloat("Move Speed", &moveSpeed_, 0.1f, 0.0f, 30.0f, "%.1f m/s");
	ImGui::DragFloat("Keep Distance", &keepDistance_, 0.1f, 0.0f, 50.0f, "%.1f m");
	// 撃破演出の粒数。configそのものは"Particle"ウィンドウ側でライブ編集できる
	ImGui::DragInt("Defeat Particles", &defeatParticleCount_, 0.5f, 0, 200);
	ImGui::End();
#endif
}

EnemySpawner::EnemySpawner() = default;

EnemySpawner::~EnemySpawner() = default;
