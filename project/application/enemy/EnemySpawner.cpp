#include "enemy/EnemySpawner.h"

#include "bullet/BulletManager.h"
#include "stage/StageData.h"
#include "math/Collision.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "audio/SoundManager.h"
#include "utility/Random.h"

#include <algorithm>
#include <cmath>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	// 敵の種別 → モデルの対応表。データ(stage.json)にはモデルパスを持たせず種別だけを持たせ、
	// 「どの見た目で出すか」はコード側のこの1箇所で決める(種別追加はここに足すだけ)
	// 現状は2種とも同じ円盤で、大きさ(ScaleOf)だけで見分ける。
	// 変数名の frog/sword はstage.jsonに書かれた種別名(データのキー)で、見た目とは無関係
	const std::string kFrogModel = "enemy/saucer.obj";  // 射撃型
	const std::string kSwordModel = "enemy/saucer.obj"; // 接近型

	// 撃破エフェクトのグループ名。configとテクスチャの登録はシーン側で行う
	const std::string kDefeatEffect = "enemyExplosion";

	// 撃破音。発射音と違い同時に何発も鳴らないので、こちらは絞らない
	const std::string kDefeatSound = "resources/sounds/explosion.mp3";
	constexpr float kDefeatVolume = 0.8f;

	// 接近音。接近型が間合いに入った合図。同時に鳴らすのは1つまでに絞る(UpdateApproachSound)
	const std::string kApproachSound = "resources/sounds/approach.mp3";
	constexpr float kApproachVolume = 0.8f;
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
	// 円盤の実寸は 約89.4 x 44.4 x 89.4(幅/高さ/奥行き)。幅が判定の直径(radius x 2)になる倍率にする
	if (enemy == "sword") {
		return 0.045f; // 接近型: 幅約4m(小さく速い)
	}
	return 0.067f;     // 射撃型: 幅約6m
}

const std::vector<std::string>& EnemySpawner::EnemyTypes() {
	static const std::vector<std::string> kTypes = { "frog", "sword" };
	return kTypes;
}

size_t EnemySpawner::TypeIndexOf(const std::string& enemy) const {
	for (size_t i = 0; i < typeParams_.size(); ++i) {
		if (typeParams_[i].name == enemy) {
			return i;
		}
	}
	// 未知の種別は既定へ(ModelPathOfと同じ扱い。データ不備でも敵が消えないので気付ける)
	return 0;
}

const EnemySpawner::TypeParams& EnemySpawner::ParamsOf(const Enemy& enemy) const {
	return typeParams_[(std::min)(enemy.typeIndex, typeParams_.size() - 1)];
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
	// 最大HPは種別ごとに違うので、色の基準もその個体の種別から引く
	const int maxHp = ParamsOf(enemy).maxHp;
	const float ratio = maxHp > 0
		? std::clamp(static_cast<float>(enemy.hp) / static_cast<float>(maxHp), 0.0f, 1.0f)
		: 1.0f;
	const float rest = 0.2f + 0.8f * ratio; // HP0付近で0.2、満タンで1.0
	enemy.object->SetColor({ 1.0f, rest, rest, 1.0f });
}

void EnemySpawner::BuildFromStage(const StageData& stageData) {
	// 撃破音は初回だけ読む(BuildFromStageはReloadのたびに呼ばれるため)
	if (!defeatSound_) {
		defeatSound_ = SoundManager::GetInstance()->LoadFile(kDefeatSound);
	}
	if (!approachSound_) {
		approachSound_ = SoundManager::GetInstance()->LoadFile(kApproachSound);
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
			if (!IsCollision(bullet.position, bulletRadius, enemy.position, ParamsOf(enemy).radius)) {
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
		if (IsCollision(playerPosition, playerRadius, enemy.position, ParamsOf(enemy).radius)) {
			playerHit_ = true;
			return;
		}
	}
}

void EnemySpawner::UpdateShooting(float deltaTime, const Vector3& playerPosition, const Vector3& playerForward) {
	if (!enemyBulletManager_) {
		return; // 発射先が無ければ撃たない(体当たりだけの挙動に戻る)
	}

	for (Enemy& enemy : enemies_) {
		const TypeParams& params = ParamsOf(enemy);
		// クールタイムは撃てない状況でも消化する(射程に入った瞬間に撃てる)
		enemy.shootCooldown -= deltaTime;
		// 撃たない種別(接近型)はここで抜ける。挙動の分岐はこの1行だけで、
		// 「どう攻めるか」の違いはパラメータの差として表している
		if (!shootEnabled_ || !params.shoot || enemy.shootCooldown > 0.0f) {
			continue;
		}

		const Vector3 toPlayer = playerPosition - enemy.position;
		const float distanceSquared = toPlayer.LengthSquared();
		if (distanceSquared > params.shootRange * params.shootRange) {
			continue; // 遠すぎる。クールタイムは減ったままにして、近づいたら撃たせる
		}
		// 自機の後ろへ回った敵は撃たない。背後からの弾は画面の外から飛んでくるので、
		// 避ける手立てがないまま当たる(射撃型は「見えている位置から撃つ」ことが成立条件)
		if (Vector3::Dot(toPlayer, playerForward) > 0.0f) {
			continue;
		}

		enemy.shootCooldown = NextShootCooldown(params);
		// 狙いは「撃った瞬間の自機の位置」。先読みはしない:
		// 自機は毎フレーム動かせるので、置き弾の方が避けて当てる遊びとして成立する
		enemyBulletManager_->Fire(enemy.position, toPlayer);
		++shotCount_;
	}
}

void EnemySpawner::UpdateApproachSound(const Vector3& playerPosition) {
	if (!approachSound_) {
		return; // 音声ファイルが無ければ鳴らさない(ゲームは止めない)
	}

	SoundManager* soundManager = SoundManager::GetInstance();
	for (Enemy& enemy : enemies_) {
		const TypeParams& params = ParamsOf(enemy);
		// 鳴らさない種別(射撃型は手前で止まるので体当たりの予告が要らない)と、鳴らし済みの個体は飛ばす
		if (enemy.approachAlerted || params.approachSoundRange <= 0.0f) {
			continue;
		}
		const float distanceSquared = (playerPosition - enemy.position).LengthSquared();
		if (distanceSquared > params.approachSoundRange * params.approachSoundRange) {
			continue;
		}

		// 範囲に入った時点で鳴らし済みにする。下で鳴らせなかった個体も、後から遅れては鳴らさない
		// (遅れて鳴ると、どの敵が近づいた合図なのか分からなくなるため)
		enemy.approachAlerted = true;

		// 同時に鳴らすのは1つまで。敵は2〜3体まとめて出るので、重ねると音が割れて
		// 「近づいてきた」という合図として聞き取れなくなる。1つ鳴っていれば合図としては足りる
		if (soundManager->IsPlaying(approachSoundHandle_)) {
			continue;
		}
		approachSoundHandle_ = soundManager->PlayWave(approachSound_, false, SoundManager::SoundCategory::SE);
		soundManager->SetVolume(approachSoundHandle_, kApproachVolume);
	}
}

float EnemySpawner::NextShootCooldown(const TypeParams& params) const {
	// 間隔に±shootIntervalRandom_の幅を掛ける。
	// 同時に出た敵が同じ周期で撃ち続けると、避けようのない斉射になるため個体差を付ける
	const float low = params.shootInterval * (1.0f - shootIntervalRandom_);
	const float high = params.shootInterval * (1.0f + shootIntervalRandom_);
	return Random::GetFloat(low, high);
}

bool EnemySpawner::IsAimedAt(const Vector3& point) const {
	// 点と球の判定。半径0の球として既存のIsCollisionに通す(専用の関数を増やさない)
	for (const Enemy& enemy : enemies_) {
		if (enemy.hp > 0 && IsCollision(point, 0.0f, enemy.position, ParamsOf(enemy).radius)) {
			return true;
		}
	}
	return false;
}

void EnemySpawner::Update(float railDistance, float deltaTime, const Vector3& playerPosition, float playerRadius,
	const Vector3& playerForward) {
	// 前フレームのコマンドはPostDrawのフェンス待ちで実行完了済みなので、退避分をここで破棄する
	graveyard_.clear();

	for (SpawnPoint& spawnPoint : spawnPoints_) {
		// 到達した瞬間に1度だけ発生させる(Resetするまで再発生しない)
		if (spawnPoint.spawned || railDistance < spawnPoint.railDistance) {
			continue;
		}
		spawnPoint.spawned = true;

		Enemy enemy;
		enemy.typeIndex = TypeIndexOf(spawnPoint.enemy);
		const TypeParams& params = typeParams_[enemy.typeIndex];
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
		enemy.hp = params.maxHp;
		// 出現と同時に撃たれると反応できないので、初回も同じ間隔だけ待たせる
		enemy.shootCooldown = NextShootCooldown(params);
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

	// 自機へ近づく。ただし種別ごとのkeepDistanceより内側には入らない。
	// 射撃型はこの距離が遠いので手前で止まって撃ち、接近型は判定半径の和より内側まで
	// 詰めるので必ず体当たりになる(=どう攻めるかが数値の差だけで決まる)
	for (Enemy& enemy : enemies_) {
		const TypeParams& params = ParamsOf(enemy);
		const Vector3 toPlayer = playerPosition - enemy.position;
		const float distance = std::sqrt(toPlayer.LengthSquared());
		if (distance > params.keepDistance && distance > 1.0e-4f) {
			// 行き過ぎて反転しないよう、残り距離を超えて進まないようにする
			const float step = (std::min)(params.moveSpeed * deltaTime, distance - params.keepDistance);
			enemy.position += Vector3::Normalized(toPlayer) * step;
			enemy.object->SetTranslate(enemy.position);
		}
		enemy.object->Update(deltaTime);
	}

	// 接近音。移動後の距離で判定するので、間合いに入ったフレームで鳴る
	UpdateApproachSound(playerPosition);

	// 攻撃。移動後に呼ぶので、弾は「今いる位置」から自機へ向けて出る
	UpdateShooting(deltaTime, playerPosition, playerForward);

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

	// 種別ごとのタブ。接近型と射撃型を別々に詰められるようにする
	// (弾の速さ・寿命・見た目は"Enemy Bullet"ウィンドウ側で調整する)
	ImGui::SeparatorText("Types");
	if (ImGui::BeginTabBar("EnemyTypes")) {
		for (TypeParams& params : typeParams_) {
			if (!ImGui::BeginTabItem(params.name.c_str())) {
				continue;
			}
			ImGui::PushID(params.name.c_str()); // 同じラベルを種別ごとに使うため
			// HPと判定半径の変更は次に発生する敵から効く(HPは発生時に確定するため)
			ImGui::DragInt("Max HP", &params.maxHp, 0.1f, 1, 20);
			ImGui::DragFloat("Hit Radius", &params.radius, 0.1f, 0.1f, 20.0f, "%.1f m");
			// 移動と攻撃は毎フレーム引き直すので、発生済みの敵にも即座に効く
			ImGui::DragFloat("Move Speed", &params.moveSpeed, 0.1f, 0.0f, 30.0f, "%.1f m/s");
			ImGui::DragFloat("Keep Distance", &params.keepDistance, 0.1f, 0.0f, 80.0f, "%.1f m");
			ImGui::Checkbox("Shoot", &params.shoot);
			ImGui::DragFloat("Fire Interval", &params.shootInterval, 0.05f, 0.1f, 10.0f, "%.2f s");
			ImGui::DragFloat("Fire Range", &params.shootRange, 1.0f, 1.0f, 300.0f, "%.0f m");
			// 0で鳴らさない。変更は鳴らしていない個体に即座に効く
			ImGui::DragFloat("Approach Sound Range", &params.approachSoundRange, 0.5f, 0.0f, 200.0f, "%.1f m");
			ImGui::PopID();
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
	}

	ImGui::SeparatorText("Common");
	ImGui::Checkbox("Shoot Enabled (all types)", &shootEnabled_);
	ImGui::SameLine();
	ImGui::Text("Shots: %d", shotCount_);
	ImGui::DragFloat("Interval Random", &shootIntervalRandom_, 0.01f, 0.0f, 0.9f, "%.2f");
	// 撃破演出の粒数。configそのものは"Particle"ウィンドウ側でライブ編集できる
	ImGui::DragInt("Defeat Particles", &defeatParticleCount_, 0.5f, 0, 200);
	ImGui::End();
#endif
}

EnemySpawner::EnemySpawner() {
	// 種別ごとの既定値。接近型と射撃型の違いはこの数値の差だけで表す
	// (挙動ごとにクラスを分けないので、種別追加はこの表に1件足すだけで済む)
	TypeParams frog;
	frog.name = "frog";
	frog.maxHp = 3;
	frog.radius = 3.0f;        // 幅約6mの円盤に合わせる(ScaleOf)
	frog.moveSpeed = 2.0f;     // ゆっくり間合いを詰める(撃つ時間を作る)
	frog.keepDistance = 22.0f; // 手前で止まり、そこから撃ち続ける
	frog.shoot = true;
	frog.shootInterval = 2.2f;
	frog.shootRange = 90.0f;

	TypeParams sword;
	sword.name = "sword";
	sword.maxHp = 2;           // 突っ込んでくる分HPは低く。撃ち落とすか避けるかの二択にする
	sword.radius = 2.0f;       // 幅約4mの円盤に合わせる(ScaleOf)
	sword.moveSpeed = 9.0f;    // 一気に間合いを詰める
	sword.keepDistance = 1.5f; // 判定半径の和(自機0.5+敵2.0=2.5m)より内側=必ず体当たりになる
	sword.shoot = false;       // 撃たない。接近そのものが攻撃
	// 9m/sで詰めてくるので、30m手前で鳴らして避ける猶予を作る
	// (敵だけなら接触の約3秒前。自機もレール上を前進するので実際はそれより短い)
	sword.approachSoundRange = 30.0f;

	typeParams_ = { frog, sword }; // 並びはEnemyTypes()と同じ
}

EnemySpawner::~EnemySpawner() = default;
