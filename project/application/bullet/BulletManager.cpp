#include "bullet/BulletManager.h"

#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "effect/ParticleManager.h"
#include "math/mathFunction.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

namespace {
	/// <summary>
	/// velocityの向きをtoTargetの向きへ、最大maxAngle[rad]だけ回す。長さ(速さ)はvelocityのまま。
	/// 角度に上限を付けるので、弾は一瞬で向き直らず、曲がりながら相手を追いかける軌跡になる
	/// </summary>
	Vector3 TurnTowards(const Vector3& velocity, const Vector3& toTarget, float maxAngle) {
		const float speed = velocity.Length();
		if (speed < 1.0e-6f || toTarget.LengthSquared() < 1.0e-6f) {
			return velocity; // 止まっている・相手と重なっている: 向きが決まらないので変えない
		}
		const Vector3 from = velocity / speed;
		const Vector3 to = Vector3::Normalized(toTarget);

		// 今の向きと目標の向きのなす角。上限以内なら目標へ向き切る
		const float angle = std::acos(std::clamp(Vector3::Dot(from, to), -1.0f, 1.0f));
		if (angle <= maxAngle) {
			return to * speed;
		}

		// 回転軸 = 今の向きと目標の向きの両方に垂直な向き(外積)。
		// 真後ろ(なす角180度)だと外積が0になり軸が決まらないので、今の向きに垂直な軸を適当に選ぶ
		// (どちら回りでも、曲がり始めれば次のフレームからは外積で軸が決まる)
		Vector3 axis = Vector3::Cross(from, to);
		if (axis.LengthSquared() < 1.0e-6f) {
			axis = Vector3::Cross(from, { 0.0f, 1.0f, 0.0f });
			if (axis.LengthSquared() < 1.0e-6f) {
				axis = Vector3::Cross(from, { 1.0f, 0.0f, 0.0f });
			}
		}
		axis.Normalize();

		// 軸まわりにmaxAngleだけ回す(ロドリゲスの回転公式)。
		// 軸がfromに垂直なので「軸方向の成分」の項は0になり、cosとsinの2項だけが残る
		const Vector3 turned = from * std::cos(maxAngle) + Vector3::Cross(axis, from) * std::sin(maxAngle);
		return Vector3::Normalized(turned) * speed;
	}
}

void BulletManager::Initialize(size_t capacity, const Profile& profile) {
	// 見た目・音・速さの設定はここで丸ごと受け取る(以降はprofile_だけを見る)
	profile_ = profile;

	// 読み込み済みなら早期returnするので重複ロードにはならない
	ModelManager::GetInstance()->LoadModel(profile_.model);

	// 発射音はSoundManagerのキャッシュと共有する
	shotSound_ = SoundManager::GetInstance()->LoadFile(profile_.shotSound);
	// 消えるときの音は指定がある弾だけ読む(空の名前で読むとエラーログが出るため)
	impactSound_ = profile_.impactSound.empty()
		? nullptr
		: SoundManager::GetInstance()->LoadFile(profile_.impactSound);

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

void BulletManager::Fire(const Vector3& position, const Vector3& direction, uint32_t targetId) {
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
		bullet.targetId = targetId;
		bullet.trailCarry = 0.0f;
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

		// 追尾。相手の今の位置へ向けて、1秒に曲がれる角度(homingTurnRate)を上限に向きを変える。
		// 位置を進める前に向きを変えるので、このフレームから曲がった向きに進む
		if (bullet.targetId != 0) {
			Vector3 targetPosition{};
			if (targetResolver_ && targetResolver_(bullet.targetId, targetPosition)) {
				// [度/秒] → このフレームで曲がれる角度[rad]。「毎秒量×dt」なのでフレームレートで曲がり方が変わらない
				const float maxTurnAngle = profile_.homingTurnRate * std::numbers::pi_v<float> / 180.0f * deltaTime;
				bullet.velocity = TurnTowards(bullet.velocity, targetPosition - bullet.position, maxTurnAngle);
			} else {
				// 相手が居なくなった(撃破・Reset)。以後は追尾をやめ、今の向きのまままっすぐ飛ぶ
				bullet.targetId = 0;
			}
		}

		// 移動。「毎秒量×dt」で書く(フレームレートが変わっても飛距離が変わらない)
		const Vector3 previousPosition = bullet.position;
		bullet.position += bullet.velocity * deltaTime;

		// 尾。1秒あたりtrailRate粒を、このフレームに進んだ区間へ等間隔に並べて出す
		// (1か所にまとめて出すと、速い弾ではフレームごとの塊が点々と並んで見える)。
		// 1粒に満たない端数は次のフレームへ持ち越すので、フレームレートが変わっても尾の濃さは変わらない
		if (!profile_.trailEffect.empty() && profile_.trailRate > 0.0f) {
			bullet.trailCarry += profile_.trailRate * deltaTime;
			const int count = static_cast<int>(bullet.trailCarry);
			bullet.trailCarry -= static_cast<float>(count);
			for (int n = 1; n <= count; ++n) {
				const float t = static_cast<float>(n) / static_cast<float>(count);
				ParticleManager::GetInstance()->Emit(
					profile_.trailEffect, Lerp(previousPosition, bullet.position, t), 1);
			}
		}

		bullet.lifeTime -= deltaTime;
		if (bullet.lifeTime <= 0.0f) {
			Expire(i); // 射程切れ。チャージ弾はここでも爆発の見た目を出す
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
				Expire(i); // 地形に当たった。消えるときの演出はProfileで指定した弾だけ出る
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
		Expire(index);
	}
}

void BulletManager::Expire(size_t index) {
	Bullet& bullet = bullets_[index];
	if (!bullet.active) {
		return; // 同じフレームに2度消されても演出を2度出さない
	}
	bullet.active = false;

	// 消えた位置に演出を出す。パーティクルはParticleManagerが寿命まで動かすので、
	// 弾(のスロット)が次の発射で使い回されても演出は残る
	if (!profile_.impactEffect.empty() && profile_.impactParticleCount > 0) {
		ParticleManager::GetInstance()->Emit(
			profile_.impactEffect, bullet.position, static_cast<uint32_t>(profile_.impactParticleCount));
	}
	if (impactSound_) {
		const SoundManager::SoundHandle handle =
			SoundManager::GetInstance()->PlayWave(impactSound_, false, SoundManager::SoundCategory::SE);
		SoundManager::GetInstance()->SetVolume(handle, profile_.impactVolume);
	}
}

bool BulletManager::HasFreeSlot() const {
	// Fireと同じ線形探索。容量が小さいので毎フレーム呼んでも問題にならない
	for (const Bullet& bullet : bullets_) {
		if (!bullet.active) {
			return true;
		}
	}
	return false;
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
	// ダメージと爆発は当たった瞬間の値を使うので、飛行中の弾にも即座に効く。
	// 爆発の巻き込みは敵側(EnemySpawner)の処理なので、敵弾のExplosion Radiusは変えても効かない
	ImGui::DragInt("Damage", &profile_.damage, 0.1f, 1, 20);
	ImGui::DragFloat("Explosion Radius", &profile_.explosionRadius, 0.1f, 0.0f, 30.0f, "%.1f m");
	// 追尾先の番号付きで撃たれた弾(今はロック中に撃ったチャージ弾だけ)に効く。飛行中の弾にも即座に効く
	ImGui::DragFloat("Homing Turn Rate", &profile_.homingTurnRate, 1.0f, 0.0f, 1080.0f, "%.0f deg/s");
	// 見た目の演出。プリセットが指定されていない弾(自弾・敵弾)では変えても何も出ない
	// (パーティクルの色や速さそのものは"Particle"ウィンドウでプリセットごとに調整する)
	ImGui::DragFloat("Trail Rate", &profile_.trailRate, 1.0f, 0.0f, 600.0f, "%.0f /s");
	ImGui::DragInt("Impact Particles", &profile_.impactParticleCount, 0.5f, 0, 300);

	if (ImGui::Button("Clear")) {
		Clear();
	}
	ImGui::End();
#endif
}

BulletManager::BulletManager() = default;

BulletManager::~BulletManager() = default;
