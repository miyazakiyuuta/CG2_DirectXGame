#include "BaseEnemy.h"
#include "../../../application/Player.h"
#include "../../../engine/3d/Object3d.h"
#include <../externals/nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <random>

BaseEnemy::BaseEnemy() = default;
BaseEnemy::~BaseEnemy() = default;

void BaseEnemy::ResolveHorizontalCollisions(const Vector3& previousPosition) {
	// 接地状態を引数に渡して処理
	ResolveHorizontalCollisionsForPos(position_, previousPosition, 1.0f, isOnGround_);
}

void BaseEnemy::ResolveVerticalCollisions() { 
	ResolveVerticalCollisionsForPos(position_, velocity_, 1.0f, 0.5f, isOnGround_); 
}

void BaseEnemy::ResolveHorizontalCollisionsForPos(Vector3& pos, const Vector3& prevPos, float radius, bool isOnGround) {
	if (!blockColliders_)
		return;
	const float kGroundEpsilon = 0.05f;

	// --------------------------------------------------------
	// 壁衝突（横方向）
	// --------------------------------------------------------
	if (pos.x != prevPos.x) {
		CollisionUtility::OBB testObb = GetOBB({pos.x, prevPos.y + kGroundEpsilon, pos.z}, radius);
		for (const auto& block : *blockColliders_) {
			if (CollisionUtility::IntersectOBB_OBB(testObb, block)) {
				pos.x = prevPos.x;
				break;
			}
		}
	}
	if (pos.z != prevPos.z) {
		CollisionUtility::OBB testObb = GetOBB({pos.x, prevPos.y + kGroundEpsilon, pos.z}, radius);
		for (const auto& block : *blockColliders_) {
			if (CollisionUtility::IntersectOBB_OBB(testObb, block)) {
				pos.z = prevPos.z;
				break;
			}
		}
	}

	// --------------------------------------------------------
	// 足場判定（着地ブロック記録方式）
	// 接地中かつ着地ブロックが記録されている場合のみ有効
	// ジャンプ中（isOnGround == false）はスキップする
	// --------------------------------------------------------
	if (isOnGround && hasLandingBlock_) {
		// 移動後の XZ 位置が着地ブロックの XZ 範囲から外れていないかチェック
		// 少しマージンを持たせてエッジで止まりやすくする
		const float kEdgeMargin = 0.1f;
		bool outsideX = pos.x < landingBlockMinX_ + kEdgeMargin || pos.x > landingBlockMaxX_ - kEdgeMargin;
		bool outsideZ = pos.z < landingBlockMinZ_ + kEdgeMargin || pos.z > landingBlockMaxZ_ - kEdgeMargin;
		if (outsideX || outsideZ) {
			// 着地ブロックの範囲外 → 横移動をキャンセルして足場に留まる
			pos.x = prevPos.x;
			pos.z = prevPos.z;
		}
	}
}


void BaseEnemy::ResolveVerticalCollisionsForPos(Vector3& pos, Vector3& vel, float collisionRadius, float visualRadius, bool& outOnGround) {
	// 初期化: 接地していない状態から開始
	outOnGround = false;

	// ジャンプ開始時（上方向に速度がある）は着地ブロック記録をクリアする
	// これにより、ジャンプ中は XZ 範囲制限が無効になる
	if (vel.y > 0.0f) {
		hasLandingBlock_ = false;
	}

	const float kSkinWidth = 0.005f;

	if (blockColliders_) {
		for (const auto& block : *blockColliders_) {
			CollisionUtility::OBB myObb = GetOBB(pos, collisionRadius);
			auto hit = CollisionUtility::IntersectOBB_OBB_Detailed(myObb, block);
			if (!hit.hit)
				continue;

			pos -= hit.normal * (hit.penetration + kSkinWidth);

			if (hit.normal.y < -0.5f) { // 床に乗った
				if (vel.y < 0.0f)
					vel.y = 0.0f;
				outOnGround = true;

				// --------------------------------------------------------
				// 着地ブロック記録：初めて着地（または未記録）のときに記録する
				// ブロックの OBB から AABB 的な XZ 範囲を計算して保存する
				// --------------------------------------------------------
				if (!hasLandingBlock_) {
					// OBB の center と halfLength（axis が軸平行を想定）から XZ 幅・上面 Y を計算
					float halfX = block.halfLength[0];
					float halfY = block.halfLength[1];
					float halfZ = block.halfLength[2];
					landingBlockMinX_ = block.center.x - halfX;
					landingBlockMaxX_ = block.center.x + halfX;
					landingBlockMinZ_ = block.center.z - halfZ;
					landingBlockMaxZ_ = block.center.z + halfZ;
					// 上面 Y = ブロック中心 Y + 半高さ（プレイヤーが乗っているかの判定に使用）
					landingBlockTopY_ = block.center.y + halfY;
					hasLandingBlock_ = true;
				}
			}
			if (hit.normal.y > 0.5f && vel.y > 0.0f) { // 天井
				vel.y = 0.0f;
			}
		}
	}

	// 地面リミット（足元にブロックが無ければ groundY_ へ落とす）
	if (!outOnGround) {
		bool blockBelow = false;
		if (blockColliders_) {
			Transform t;
			t.translate = {pos.x, pos.y - 30.0f, pos.z};
			t.rotate = object_ ? object_->GetRotate() : Vector3{0, 0, 0};
			CollisionUtility::OBB belowObb = CollisionUtility::MakeOBBFromTransform(t, {collisionRadius, 30.0f, collisionRadius});
			for (const auto& block : *blockColliders_) {
				if (CollisionUtility::IntersectOBB_OBB(belowObb, block)) {
					blockBelow = true;
					break;
				}
			}
		}
		if (!blockBelow) {
			float floorLimit = groundY_ + collisionRadius;
			if (pos.y < floorLimit) {
				pos.y = floorLimit;
				if (vel.y < 0.0f)
					vel.y = 0.0f;
				outOnGround = true;
			}
		}
	}
}


void BaseEnemy::SetColor(const Vector4& color) {
	if (object_)
		object_->SetColor(color);
	originalColor_ = color;
}

void BaseEnemy::SetAlpha(float a) {
	if (object_) {
		Vector4 c = originalColor_;
		c.w = a;
		object_->SetColor(c);
	}
}

float BaseEnemy::GetOriginalAlpha() const { return originalColor_.w; }

CollisionUtility::OBB BaseEnemy::GetOBB(const Vector3& pos, float radius) const {
	Transform t;
	t.translate = pos;
	t.rotate = object_ ? object_->GetRotate() : Vector3{0, 0, 0};
	return CollisionUtility::MakeOBBFromTransform(t, {radius, radius, radius});
}

void BaseEnemy::LoadModel(const std::string& filePath) {
    modelPath_ = filePath;
    if (object_) {
        object_->SetModel(filePath);
    }
}

void BaseEnemy::UpdateDeathAnimation(float deltaTime) {
    if (!soulObject_ && object_ && common_ && camera_) {
        // 魂オブジェクトを本体と同じ状態で初期化
        soulObject_ = std::make_unique<Object3d>();
        soulObject_->Initialize(common_);
        soulObject_->SetModel(modelPath_.empty() ? "Cube.obj" : modelPath_);
        soulObject_->SetCamera(camera_);
        soulObject_->SetTranslate(object_->GetTranslate());
        soulObject_->SetScale(object_->GetScale());
        soulObject_->SetRotate(object_->GetRotate());
        soulObject_->SetColor({originalColor_.x, originalColor_.y, originalColor_.z, 1.0f});

        // 本体（死体）を倒す（X軸かZ軸に90度）
        Vector3 rot = object_->GetRotate();
        rot.x += 1.57f;
        object_->SetRotate(rot);
        object_->Update();
    }

    deathTimer_ += deltaTime;

    if (soulObject_) {
        // 魂を上に昇らせる
        Vector3 soulPos = soulObject_->GetTranslate();
        soulPos.y += deltaTime * 2.0f;
        soulObject_->SetTranslate(soulPos);

        // アルファ値を下げて透明にする（1.5秒かけて0にする）
        float alpha = std::clamp(1.0f - (deathTimer_ / 1.5f), 0.0f, 1.0f);
        soulObject_->SetColor({originalColor_.x, originalColor_.y, originalColor_.z, alpha});

        // スピニングなど少し回転させると魂っぽい
        Vector3 rot = soulObject_->GetRotate();
        rot.y += deltaTime * 2.0f;
        soulObject_->SetRotate(rot);

        soulObject_->Update();
    }

    // 死体（本体オブジェクト）も毎フレームUpdateしないと画面にくっついてしまうため呼ぶ
    if (object_) {
        object_->Update();
    }

    if (deathTimer_ >= 1.5f) {
        isDestroyed_ = true; // 完全に透明になったら削除許可
    }
}

void BaseEnemy::DrawDeathAnimation() {
    if (object_) {
        object_->Draw();
    }
    if (soulObject_) {
        soulObject_->Draw();
    }
}

int BaseEnemy::DistributeDrops() {
	if (dropTable_.empty() || !player_)
		return 0;
	std::vector<float> weights;
	std::vector<const DropEntry*> entries;
	std::random_device rd;
	std::mt19937 gen(rd());
	for (const auto& d : dropTable_) {
		std::uniform_real_distribution<float> chanceDist(0.0f, 1.0f);
		if (d.chance >= 1.0f || chanceDist(gen) <= d.chance) {
			if (d.weight > 0.0f)
				weights.push_back(d.weight);
			else
				weights.push_back(0.0f);
			entries.push_back(&d);
		}
	}
	if (entries.empty())
		return 0;
	std::discrete_distribution<size_t> dist(weights.begin(), weights.end());
	size_t idx = dist(gen);
	const DropEntry* chosen = entries[idx];
	if (!chosen)
		return 0;
	int amount = chosen->minAmount;
	if (chosen->maxAmount > chosen->minAmount) {
		std::uniform_int_distribution<int> amtDist(chosen->minAmount, chosen->maxAmount);
		amount = amtDist(gen);
	}
	lastDropAbility_ = chosen->ability;
	return amount;
}

bool BaseEnemy::HasGroundBelow(const Vector3& pos, float checkHeight) const {
	if (!blockColliders_)
		return false;
	Transform t;
	t.translate = {pos.x, pos.y - checkHeight * 0.5f, pos.z};
	t.rotate = object_ ? object_->GetRotate() : Vector3{0, 0, 0};
	CollisionUtility::OBB groundObb = CollisionUtility::MakeOBBFromTransform(t, {1.0f, checkHeight, 1.0f});
	for (const auto& block : *blockColliders_) {
		if (CollisionUtility::IntersectOBB_OBB(groundObb, block))
			return true;
	}
	return false;
}
