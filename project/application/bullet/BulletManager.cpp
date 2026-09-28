#include "bullet/BulletManager.h"

#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void BulletManager::Initialize(size_t capacity, const Profile& profile) {
	// 見た目・音・速さの設定はここで丸ごと受け取る(以降はprofile_だけを見る)
	profile_ = profile;

	// 読み込み済みなら早期returnするので重複ロードにはならない
	ModelManager::GetInstance()->LoadModel(profile_.model);

	// 発射音はSoundManagerのキャッシュと共有する
	shotSound_ = SoundManager::GetInstance()->LoadFile(profile_.shotSound);

	bullets_.assign(capacity, Bullet{});

	// Object3dはここで作り切る。以降Fire/Killでは作らない・消さないので、
	// 描画コマンドが参照中の実体が消えることがない(graveyardが要らない理由)
	objects_.clear();
	objects_.reserve(capacity);
	for (size_t i = 0; i < capacity; ++i) {
		std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
		object->Initialize(Object3dCommon::GetInstance());
		object->SetModel(profile_.model);
		// sphere.objは半径0.5なので、判定半径と見た目を一致させる倍率にする
		const float scale = profile_.radius * 2.0f;
		object->SetScale({ scale, scale, scale });
		object->SetEnableLighting(false); // 陰影を付けず、光っている弾に見せる
		object->SetColor(profile_.color);
		if (camera_) {
			object->SetCamera(camera_);
		}
		objects_.push_back(std::move(object));
	}
}

void BulletManager::Fire(const Vector3& position, const Vector3& direction) {
	// 空きスロットを線形探索する。上限64程度なら毎フレーム舐めても問題にならない
	// (空きindexのスタックを持つ手もあるが、GPUパーティクルと違い桁が小さいので単純さを取る)
	for (size_t i = 0; i < bullets_.size(); ++i) {
		if (bullets_[i].active) {
			continue;
		}

		Bullet& bullet = bullets_[i];
		bullet.position = position;
		// 方向が縮退(長さ0)していると速度が0になり、その場に留まる弾ができるため弾かない
		Vector3 velocity = direction;
		if (velocity.LengthSquared() < 1.0e-6f) {
			return;
		}
		bullet.velocity = velocity.Normalize() * profile_.speed;
		bullet.lifeTime = profile_.lifeTime;
		bullet.active = true;

		// 発射フレームから正しい位置に出るよう、実体の座標もここで合わせる
		objects_[i]->SetTranslate(bullet.position);
		++firedCount_;

		// 発射音。空きが無くて撃てなかった場合は鳴らない(この行に到達しない)ので、
		// 「音はしたのに弾が出ない」というズレが起きない
		if (shotSound_) {
			const SoundManager::SoundHandle handle =
				SoundManager::GetInstance()->PlayWave(shotSound_, false, SoundManager::SoundCategory::SE);
			SoundManager::GetInstance()->SetVolume(handle, profile_.shotVolume);
		}
		return;
	}
	// 空きが無い場合は発射しない(上限に張り付いていることはImGuiのAliveで分かる)
}

void BulletManager::Update(float deltaTime) {
	aliveCount_ = 0;

	for (size_t i = 0; i < bullets_.size(); ++i) {
		Bullet& bullet = bullets_[i];
		if (!bullet.active) {
			continue; // 死んでいる弾はObject3d::Updateもしない(Drawもしないので行列は古いままでよい)
		}

		// 等速直進。「毎秒量×dt」で書く(フレームレートが変わっても飛距離が変わらない)
		bullet.position += bullet.velocity * deltaTime;

		bullet.lifeTime -= deltaTime;
		if (bullet.lifeTime <= 0.0f) {
			bullet.active = false;
			continue;
		}

		// 地形(stage.jsonのコライダー)との判定。プレイヤーと同じ「AABB vs 球」を総当たりで使う。
		// これが無いと弾が岩や建造物を貫通して飛び続け、撃った手応えが確認できない
		if (stageColliders_) {
			bool hitTerrain = false;
			for (const AABB& aabb : *stageColliders_) {
				if (IsCollision(aabb, bullet.position, profile_.radius)) {
					hitTerrain = true;
					break;
				}
			}
			if (hitTerrain) {
				// A-4ではここがヒットエフェクト(ParticleManagerのプリセット)の発生点になる
				bullet.active = false;
				++terrainHitCount_;
				continue;
			}
		}

		objects_[i]->SetTranslate(bullet.position);
		objects_[i]->Update(deltaTime);
		++aliveCount_;
	}
}

void BulletManager::Draw() {
	for (size_t i = 0; i < bullets_.size(); ++i) {
		if (bullets_[i].active) {
			objects_[i]->Draw();
		}
	}
}

void BulletManager::SetCamera(Camera* camera) {
	camera_ = camera;
	for (std::unique_ptr<Object3d>& object : objects_) {
		object->SetCamera(camera);
	}
}

void BulletManager::Clear() {
	// 実体は残したままフラグを倒すだけ。ImGuiのボタン(Draw後)から呼ばれても安全
	for (Bullet& bullet : bullets_) {
		bullet.active = false;
	}
	aliveCount_ = 0;
}

void BulletManager::Kill(size_t index) {
	if (index < bullets_.size()) {
		bullets_[index].active = false;
	}
}

void BulletManager::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin(profile_.debugName.c_str());
	// Aliveがプール上限に張り付いていたら、発射しても撃てていない状態
	ImGui::Text("Alive: %zu / %zu", aliveCount_, bullets_.size());
	ImGui::Text("Fired: %d  TerrainHit: %d", firedCount_, terrainHitCount_);

	ImGui::SeparatorText("Parameters");
	// 変更は次に発射する弾から効く(飛行中の弾のvelocityは発射時に確定済み)
	ImGui::DragFloat("Speed", &profile_.speed, 1.0f, 1.0f, 500.0f, "%.0f m/s");
	ImGui::DragFloat("Life Time", &profile_.lifeTime, 0.05f, 0.05f, 10.0f, "%.2f s");
	// 速度×寿命=射程。調整の目安に出す
	ImGui::Text("Range: %.0f m", profile_.speed * profile_.lifeTime);
	if (ImGui::DragFloat("Radius", &profile_.radius, 0.01f, 0.01f, 5.0f, "%.2f m")) {
		// 判定半径を変えたら見た目も合わせる(sphere.objは半径0.5)
		const float scale = profile_.radius * 2.0f;
		for (std::unique_ptr<Object3d>& object : objects_) {
			object->SetScale({ scale, scale, scale });
		}
	}

	if (ImGui::Button("Clear")) {
		Clear();
	}
	ImGui::End();
#endif
}

BulletManager::BulletManager() = default;

BulletManager::~BulletManager() = default;
