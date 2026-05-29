#include "ShootingEnemy.h"
#include "../../../engine/3d/Object3d.h"
#include "../../Player.h"
#include <cmath>

ShootingEnemy::ShootingEnemy() = default;
ShootingEnemy::~ShootingEnemy() = default;

void ShootingEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	LoadModel("ShootingEnemy.obj");
	object_->SetCamera(camera);
	position_ = pos;

	float scaleY = 0.8f;
    object_->SetScale({0.8f, scaleY, 0.8f});

	groundY_ = scaleY;
}

// プレイヤーへの視線がブロックで遮られているかチェック
// 敵→プレイヤーにレイを飛ばし、途中にブロックがあれば true（遮蔽あり）
bool ShootingEnemy::IsLineOfSightBlocked(const Vector3& target) const {
	if (!blockColliders_) {
		return false;
	}

	// 敵からプレイヤーへの方向ベクトル
	Vector3 toTarget = target - position_;
	float distToPlayer = std::sqrt(
		toTarget.x * toTarget.x +
		toTarget.y * toTarget.y +
		toTarget.z * toTarget.z
	);

	// 距離が0なら遮蔽なし
	if (distToPlayer < 0.01f) {
		return false;
	}

	// レイの方向を正規化
	Vector3 dir = {
		toTarget.x / distToPlayer,
		toTarget.y / distToPlayer,
		toTarget.z / distToPlayer
	};

	// レイを構築
	CollisionUtility::Ray ray;
	ray.origin = position_;
	ray.dir = dir;

	// 全ブロックに対してレイキャスト
	for (const auto& block : *blockColliders_) {
		float t = 0.0f;
		if (CollisionUtility::RayIntersectOBB(ray, block, &t)) {
			// ブロックとの衝突点がプレイヤーよりも手前にあれば遮蔽
			if (t > 0.0f && t < distToPlayer) {
				return true;
			}
		}
	}

	return false;
}

void ShootingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	// 弾の更新（死亡後も飛ばし続ける）
	for (auto it = bullets_.begin(); it != bullets_.end();) {
		(*it)->Update(deltaTime);
		if ((*it)->IsDead())
			it = bullets_.erase(it);
		else
			++it;
	}

	if (isDead_) {
		UpdateDeathAnimation(deltaTime);
		return;
	}

	// 1. 水平移動前の座標を保持
	Vector3 previousPosition = position_;

	// --- 感知判定 ---
	// プレイヤーまでの水平距離と垂直距離を計算
	Vector3 toPlayer = playerPos - position_;
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
	float distY = std::abs(toPlayer.y);

	// 水平・垂直ともに感知範囲内かチェック
	bool inRange = (distXZ <= detectionRange_) && (distY <= detectionHeightRange_);

	// 範囲内ならブロックによる視線遮蔽をチェック
	if (inRange) {
		isPlayerDetected_ = !IsLineOfSightBlocked(playerPos);
	} else {
		isPlayerDetected_ = false;
	}

	// --- プレイヤー方向への回転 ---
	// 感知中はプレイヤーの方向を向く
	if (isPlayerDetected_ && object_) {
		float targetYaw = std::atan2(toPlayer.x, toPlayer.z);
		Vector3 rot = object_->GetRotate();
		rot.y = targetYaw;
		object_->SetRotate(rot);
	}

	// --- 射撃タイマー ---
	// 感知中かつ擬態していない場合のみ射撃
	if (isPlayerDetected_ && (!player_ || !player_->IsMimicking())) {
		shotTimer_ += deltaTime;
		if (shotTimer_ >= kShotInterval) {
			Shoot(playerPos);
			shotTimer_ = 0.0f;
		}
	} else {
		// 感知していない場合はタイマーをリセット
		shotTimer_ = 0.0f;
	}

	// 2. 水平移動の解決（移動がなくても壁判定のために呼ぶ）
	ResolveHorizontalCollisions(previousPosition);

	// 3. 垂直移動の計算
	velocity_.y += gravity_; // 重力加算
	position_.y += velocity_.y;

	// 4. 垂直衝突解決（接地判定）
	ResolveVerticalCollisions();

	if (object_) {
		object_->SetTranslate(position_);
	}
}

void ShootingEnemy::Shoot(const Vector3& target) {
	auto b = std::make_unique<EnemyBullet>();
	b->Initialize(
		common_,
		camera_,
		position_,
		target,
		GetProjectileDamage()
	);
	bullets_.push_back(std::move(b));
}

void ShootingEnemy::Draw() {
	object_->Update();
	if (isDead_) {
		DrawDeathAnimation();
	} else if (object_) {
		object_->Draw();
	}

	for (auto& b : bullets_)
		b->Draw();
}