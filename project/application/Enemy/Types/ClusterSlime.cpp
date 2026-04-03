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
}

void ClusterSlime::Update(float deltaTime, const Vector3& playerPos) {
	const float kCollisionRadius = 0.35f;
	const float kVisualRadius = 0.2f;

	// 【追加】減速計算用の定数
	const float kSlowDetectionRadius = 5.0f;  // プレイヤーにまとわりついていると判定する距離
	const float kSlowEffectPerMember = 0.1f; // 1匹につき5%減速
	int surroundCount = 0;

	for (size_t i = 0; i < members_.size(); ++i) {
		auto& m = members_[i];
		Vector3 previousPos = m.position;

		Vector3 accel = {0, 0, 0};
		Vector3 toPlayer = playerPos - m.position;
		toPlayer.y = 0;
		float distToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

		// 【追加】個体ごとにプレイヤーとの距離をチェック
		if (distToPlayer < kSlowDetectionRadius) {
			surroundCount++;
		}

		if (distToPlayer < detectRadius_) {
			Vector3 dir = Vector3::Normalized(toPlayer);
			accel.x += dir.x * 0.015f;
			accel.z += dir.z * 0.015f;
		}

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
		m.velocity.x *= 0.85f;
		m.velocity.z *= 0.85f;

		m.position.x += m.velocity.x;
		m.position.z += m.velocity.z;

		ResolveHorizontalCollisionsForPos(m.position, previousPos, kCollisionRadius);
		m.velocity.y += gravity_;
		m.position.y += m.velocity.y;
		ResolveVerticalCollisionsForPos(m.position, m.velocity, kCollisionRadius, kVisualRadius, m.onGround);

		m.timer += deltaTime * 8.0f;
		float jumpOffset = 0.0f;
		if (m.onGround) {
			jumpOffset = std::abs(std::sin(m.timer * 4.0f)) * 0.3f;
		}

		Vector3 renderPos = m.position;
		renderPos.y += jumpOffset;

		m.object->SetTranslate(renderPos);
		m.object->Update();
	}

	// 【重要】累積減速倍率を計算（15体なら 1.0 - 0.75 = 0.25倍速 になる）
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