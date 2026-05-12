#pragma once
#include "../../../engine/math/Vector3.h"
#include "../../../engine/utility/CollisionUtility.h"
#include "math/Vector4.h"
#include <memory>
#include <vector>
#include <string>
#include "../../Ability.h"
#include <random>
#include <unordered_map>

class Object3d;
class Object3dCommon;
class Camera;
class Player;

class BaseEnemy {
public:
	BaseEnemy();
	virtual ~BaseEnemy();

	virtual void Initialize(Object3dCommon* common, Camera* camera, const Vector3& pos) = 0;
	virtual void Update(float deltaTime, const Vector3& playerPos) = 0;
	virtual void Draw() = 0;

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* colliders) { blockColliders_ = colliders; }

	const Vector3& GetPosition() const { return position_; }
	bool IsDead() const { return isDead_; }
	void Kill() { isDead_ = true; }

    // 色設定ヘルパー：派生クラスは Initialize 内で SetColor を呼ぶこと
	void SetColor(const Vector4& color);
	void SetAlpha(float a);
   // 元のアルファ値を返す（SetColor 呼び出し時に保存される）
	float GetOriginalAlpha() const;

	// プレイヤーへの速度補正値を取得
	float GetPlayerSpeedMultiplier() const { return playerSpeedMultiplier_; }

	// 4. マネージャーからプレイヤー情報を受け取るための追加
	void SetPlayer(class Player* p) { player_ = p; }

   // 能力 XP のドロップ定義
  struct DropEntry {
		AbilityId ability = AbilityId::Unknown;
		float weight = 1.0f; // sampling weight
		int minAmount = 1;
		int maxAmount = 1;
		float chance = 1.0f; // per-entry chance (0..1)
	};

   // この敵インスタンス用のドロップテーブルを設定
	void SetDropTable(const std::vector<DropEntry>& table) { dropTable_ = table; }
    // 添付されたプレイヤーへドロップ配布（死亡時に呼ばれる）
	// 戻り値: 配布した総ドロップ量（整数）。0 の場合はドロップ無し。
	int DistributeDrops();
	AbilityId GetLastDropAbility() const { return lastDropAbility_; }

	// ソナー（エコー）が当たった時に呼ばれる
	virtual void OnSonarHit() {}

	// 舌で掴める（フックできる）対象かどうかを返す。デフォルトは false。
	// フェイズゴーストなどの特殊な敵だけ、状態に合わせて true を返すようにオーバーライドする。
	virtual bool IsGrappable() const { return false; }

	// 【修正】当たり判定用のOBB取得を public に移動し、Playerから参照可能にする
	virtual CollisionUtility::OBB GetOBB(const Vector3& pos, float radius) const;

	// 舌が当たった時の通知（当たった方向を受け取る）
	virtual void OnTongueHit(const Vector3& direction) { (void)direction; }

	// 慣性ジャンプ用に現在の移動速度を取得する
	virtual Vector3 GetVelocity() const { return velocity_; }



protected:
	void ResolveHorizontalCollisions(const Vector3& previousPosition);
	void ResolveVerticalCollisions();

	void ResolveHorizontalCollisionsForPos(Vector3& pos, const Vector3& prevPos, float radius) const;
	void ResolveVerticalCollisionsForPos(Vector3& pos, Vector3& vel, float collisionRadius, float visualRadius, bool& outOnGround) const;

protected:
	std::unique_ptr<Object3d> object_ = nullptr;
	Vector4 originalColor_ = {1.0f, 1.0f, 1.0f, 1.0f};
	Vector3 position_ = {0.0f, 0.0f, 0.0f};
	Vector3 velocity_ = {0.0f, 0.0f, 0.0f};
	bool isDead_ = false;
	bool isOnGround_ = false;

	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;

	float gravity_ = -0.04f;
	float groundY_ = 0.0f;

	// プレイヤーの移動速度倍率（1.0が通常）
	float playerSpeedMultiplier_ = 1.0f;

	class Player* player_ = nullptr;
    std::vector<DropEntry> dropTable_;
  AbilityId lastDropAbility_ = AbilityId::Unknown;
};