#include "ChasingEnemy.h"
#include "../../../engine/3d/Object3d.h"
#include "../../Player.h"
#include <algorithm>
#include <cmath>
#include <limits>

ChasingEnemy::ChasingEnemy() = default;
ChasingEnemy::~ChasingEnemy() = default;

void ChasingEnemy::Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) {
	common_ = common;
	camera_ = camera;
	object_ = std::make_unique<Object3d>();
	object_->Initialize(common);
	LoadModel("ChasingEnemy.obj");
	object_->SetCamera(camera);
	position_ = pos;

	float scale = 1.0f;
    object_->SetScale({scale, scale, scale});

	// 最低地面の基準を 0.0f に設定
	groundY_ = 0.0f;
	gravity_ = -0.025f;

	// 出現時に即座に埋まらないよう、高めに配置
	position_.y = (std::max)(position_.y, 2.5f);
}

// プレイヤーに最も近い、ジャンプで到達可能な足場を探す
// maxJumpVelocity_ から到達可能な最大高さを計算し、
// その範囲内でプレイヤーとの3D距離が最も近いブロックを選ぶ
bool ChasingEnemy::FindBestPlatformToward(const Vector3& playerPos, float& targetBlockTop) const {
	if (!blockColliders_) {
		return false;
	}

	// maxJumpVelocity_ で到達できる最大高さを物理計算
	// v^2 = 2 * |gravity| * h  →  h = v^2 / (2 * |gravity|)
	float maxJumpHeight = (maxJumpVelocity_ * maxJumpVelocity_) / (2.0f * std::abs(gravity_));

	bool found = false;
	float bestDistToPlayer = (std::numeric_limits<float>::max)();

	for (const auto& block : *blockColliders_) {
		// ブロックの上面Y座標
		float blockTopY = block.center.y + block.halfLength[1];

		// 自分より高い足場のみ対象（低い足場は追跡に不要）
		if (blockTopY <= position_.y + 0.3f) {
			continue;
		}

		// ジャンプで届く高さかチェック
		float heightDiff = blockTopY - position_.y;
		if (heightDiff > maxJumpHeight) {
			continue;
		}

		// ブロック上面の中心座標（着地目標地点）
		Vector3 blockSurface = {
			block.center.x,
			blockTopY,
			block.center.z
		};

		// プレイヤーとの3D距離を計算
		Vector3 toPlayer = playerPos - blockSurface;
		float distSq = toPlayer.x * toPlayer.x +
		               toPlayer.y * toPlayer.y +
		               toPlayer.z * toPlayer.z;

		// 最もプレイヤーに近いブロックを選択
		if (distSq < bestDistToPlayer) {
			bestDistToPlayer = distSq;
			targetBlockTop = blockTopY;
			found = true;
		}
	}

	return found;
}

void ChasingEnemy::Update(float deltaTime, const Vector3& playerPos) {
	if (isDead_) {
		UpdateDeathAnimation(deltaTime);
		return;
	}

	if (!object_)
		return;

	Vector3 prevPos = position_;
	Vector3 toPlayer = playerPos - position_;

	// --- 感知判定 ---
	// 水平距離と垂直距離を計算
	float distXZ = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
	float distY = std::abs(toPlayer.y);

	// 感知範囲内かチェック（水平＋垂直）
	bool inRange = (distXZ <= detectionRange_) && (distY <= detectionHeightRange_);

	// 擬態中のプレイヤーには反応しない
	bool playerVisible = inRange && (!player_ || !player_->IsMimicking());

	// --- 水平移動 ---
	// 感知範囲内の場合のみ追跡
	if (playerVisible && distXZ > 1.0f) {
		// 水平方向のプレイヤーへのベクトル
		Vector3 horizontalToPlayer = toPlayer;
		horizontalToPlayer.y = 0.0f;
		Vector3 moveDir = Vector3::Normalized(horizontalToPlayer);

		// 水平移動速度を 60FPS 基準に補正
		float actualSpeed = (speed_ > 1.0f) ? (speed_ / 60.0f) : speed_;
		position_.x += moveDir.x * actualSpeed;
		position_.z += moveDir.z * actualSpeed;
	}

	// --- プレイヤー方向への回転 ---
	// 追跡中はプレイヤーの方向を向く
	if (playerVisible && object_) {
		float targetYaw = std::atan2(toPlayer.x, toPlayer.z);
		Vector3 rot = object_->GetRotate();
		rot.y = targetYaw;
		object_->SetRotate(rot);
	}

	// 1. 水平衝突解決
	ResolveHorizontalCollisions(prevPos);

	// 2. ジャンプAI
	if (playerVisible && isOnGround_) {
		// プレイヤーが上にいる場合のジャンプ処理
		if (playerPos.y > position_.y + 0.5f && distXZ < jumpRange_) {
			// プレイヤーに最も近い到達可能な足場を探す
			float targetBlockTop = 0.0f;
			bool foundPlatform = FindBestPlatformToward(playerPos, targetBlockTop);

			if (foundPlatform) {
				// 足場の高さに届くジャンプ力を計算
				float heightToBlock = targetBlockTop - position_.y;
				// 少し余裕を持たせる（1.3倍）
				float requiredV = std::sqrt(2.0f * std::abs(gravity_) * heightToBlock * 1.3f);
				velocity_.y = (std::clamp)(requiredV, 0.3f, maxJumpVelocity_);
				isOnGround_ = false;
			} else {
				// 足場が見つからない場合は直接プレイヤーに向かって制限付きジャンプ
				float heightDiff = playerPos.y - position_.y;
				float requiredV = std::sqrt(2.0f * std::abs(gravity_) * heightDiff * 1.3f);
				velocity_.y = (std::clamp)(requiredV, 0.3f, maxJumpVelocity_);
				isOnGround_ = false;
			}
		}
		// プレイヤーが同じ高さ or 低い位置にいても、間に障害物があればジャンプ
		else if (distXZ < jumpRange_ && distXZ > 2.0f) {
			// 前方に壁があるか簡易チェック（水平移動が止められた = 壁）
			Vector3 afterCollision = position_;
			if (std::abs(afterCollision.x - prevPos.x) < 0.001f &&
				std::abs(afterCollision.z - prevPos.z) < 0.001f &&
				distXZ > 2.0f) {
				// 壁にぶつかったので小ジャンプで乗り越える
				velocity_.y = (std::clamp)(0.5f, 0.3f, maxJumpVelocity_);
				isOnGround_ = false;
			}
		}
	}

	// 3. 垂直移動
	velocity_.y += gravity_;
	position_.y += velocity_.y;

	// 4. 垂直衝突解決（ブロックの上に乗る処理含む）
	ResolveVerticalCollisions();

	object_->SetTranslate(position_);
}

void ChasingEnemy::Draw() {
	object_->Update();
	if (isDead_) {
		DrawDeathAnimation();
		return;
	}

	if (object_)
		object_->Draw();
}