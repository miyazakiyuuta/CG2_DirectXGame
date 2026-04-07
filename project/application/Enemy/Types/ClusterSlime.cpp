#include "ClusterSlime.h"
#include "../../../engine/3d/Object3d.h"
#include <algorithm>
#include <cmath>
#include <random>

ClusterSlime::ClusterSlime() = default;
ClusterSlime::~ClusterSlime() = default;

void ClusterSlime::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	object_->SetModel("sphere.obj");
	object_->SetCamera(camera);
	object_->SetScale({0.1f, 0.1f, 0.1f});
	object_->SetColor({1, 1, 1, 0});

	std::random_device seed_gen;
	std::mt19937 engine(seed_gen());
	std::uniform_real_distribution<float> distPos(-4.0f, 4.0f);

	for (int i = 0; i < 15; ++i) {
		Member m;
		m.object = std::make_unique<Object3d>();
		m.object->Initialize(common);
		m.object->SetModel("sphere.obj");
		m.object->SetCamera(camera);
		m.object->SetScale({0.4f, 0.4f, 0.4f});
		m.object->SetColor({0.9f, 0.0f, 0.2f, 1.0f});

		m.position = {pos.x + distPos(engine), pos.y + 5.0f, pos.z + distPos(engine)};
		m.velocity = {0, 0, 0};
		m.timer = distPos(engine);
		m.onGround = false;

		members_.push_back(std::move(m));
	}

	groundY_ = 0.0f;
	gravity_ = -0.025f; // 重力設定
}

void ClusterSlime::Update(float deltaTime, const Vector3& playerPos) {
	const float kCollisionRadius = 0.35f;
	const float kVisualRadius = 0.2f;

	const float kSlowDetectionRadiusXZ = 2.5f; // 水平の判定距離
	const float kSlowDetectionRadiusY = 3.0f;  // 垂直（高さ）の判定距離。
	const float kSlowEffectPerMember = 0.05f;
	int surroundCount = 0;

	for (size_t i = 0; i < members_.size(); ++i) {
		auto& m = members_[i];
		Vector3 previousPos = m.position;

		Vector3 toPlayer = playerPos - m.position;
		float distToPlayerXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
		float heightDiff = std::abs(playerPos.y - m.position.y);

		// --- 1. デバフ判定 ---
		if (distToPlayerXZ < kSlowDetectionRadiusXZ && heightDiff < kSlowDetectionRadiusY) {
			surroundCount++;
		}

		// --- 2. ジャンプAI（浮遊足場対応版） ---
		if (m.onGround) {
			float jumpHeightDiff = playerPos.y - m.position.y;
			// プレイヤーが自分より高い位置（0.3m以上）にいて、届く範囲（6.0m以内）ならジャンプを検討
			// 水平距離も考慮し、少し離れていても飛びつくようにする
			if (jumpHeightDiff > 0.3f && jumpHeightDiff < 6.0f && distToPlayerXZ < 5.0f) {
				// 垂直速度の計算: v = sqrt(2gh) に少し余裕を持たせる
				float requiredV = std::sqrt(2.0f * std::abs(gravity_) * jumpHeightDiff * 1.6f);
				m.velocity.y = (std::clamp)(requiredV, 0.3f, 0.75f);

				// 【改善】ジャンプした瞬間にプレイヤーの方向へ水平初速を与える（飛び出し）
				if (distToPlayerXZ > 0.1f) {
					float leapForce = 0.15f; // 飛び出しの強さ
					m.velocity.x += (toPlayer.x / distToPlayerXZ) * leapForce;
					m.velocity.z += (toPlayer.z / distToPlayerXZ) * leapForce;
				}

				m.onGround = false;
			}
		}

		// --- 3. 移動計算 ---
		Vector3 accel = {0, 0, 0};
		if (distToPlayerXZ < detectRadius_) {
			Vector3 dir = {toPlayer.x, 0.0f, toPlayer.z};
			float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
			if (len > 0.001f) {
				dir.x /= len;
				dir.z /= len;

				// 地面にいる時と空中での加速を変える
				float accelStr = m.onGround ? 0.015f : 0.008f; // 空中でも少しだけ誘導をかける
				accel.x += dir.x * accelStr;
				accel.z += dir.z * accelStr;
			}
		}

		// 群れの分離（はびこる挙動）
		for (size_t j = 0; j < members_.size(); ++j) {
			if (i == j)
				continue;
			Vector3 diff = m.position - members_[j].position;
			diff.y = 0;
			float dist = std::sqrt(diff.x * diff.x + diff.z * diff.z);
			if (dist > 0 && dist < personalSpace_) {
				float force = (personalSpace_ - dist) / personalSpace_;
				accel.x += (diff.x / dist) * force * 0.025f;
				accel.z += (diff.z / dist) * force * 0.025f;
			}
		}

		m.velocity.x += accel.x;
		m.velocity.z += accel.z;

		// 摩擦（接地時と空中で分ける）
		float friction = m.onGround ? 0.85f : 0.95f; // 空中では慣性を残す
		m.velocity.x *= friction;
		m.velocity.z *= friction;

		m.position.x += m.velocity.x;
		m.position.z += m.velocity.z;

		// 衝突解決（水平）
		ResolveHorizontalCollisionsForPos(m.position, previousPos, kCollisionRadius);

		// 垂直移動
		m.velocity.y += gravity_;
		m.position.y += m.velocity.y;

		// 衝突解決（垂直：これでブロックの上に乗る）
		ResolveVerticalCollisionsForPos(m.position, m.velocity, kCollisionRadius, kVisualRadius, m.onGround);

		// アニメーション
		m.timer += deltaTime * 8.0f;
		float jumpOffset = m.onGround ? std::abs(std::sin(m.timer * 4.0f)) * 0.3f : 0.0f;
		m.object->SetTranslate({m.position.x, m.position.y + jumpOffset, m.position.z});
		m.object->Update();
	}

	// 4. 累積減速倍率を計算
	playerSpeedMultiplier_ = (std::clamp)(1.0f - (surroundCount * kSlowEffectPerMember), 0.2f, 1.0f);

	if (object_ && !members_.empty()) {
		Vector3 avg = {0, 0, 0};
		for (auto& mem : members_)
			avg += mem.position;
		position_ = avg * (1.0f / (float)members_.size());
		object_->SetTranslate(position_);
		object_->Update();
	}
}

void ClusterSlime::Draw() {
	for (auto& m : members_) {
		m.object->Draw();
	}
}