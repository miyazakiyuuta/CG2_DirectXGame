#pragma once
#include "utility/CollisionUtility.h"
#include "../Core/BaseEnemy.h"
#include <functional>
#include <list>
#include <memory>
#include <unordered_map>
#include <vector>

class BaseEnemy;
class Object3dCommon;
class Camera;
struct Vector3;
enum class EnemyType { 
	Chasing,
	Shooting,
	Sentinel,
	ClusterSlime, 
	ProminenceSensor, 
	PhaseGhost 
};

class EnemyManager {
public:
	EnemyManager();
	~EnemyManager();

	void Initialize(Object3dCommon* common, Camera* camera);
	void Update(float deltaTime, class Player* player);
	void Draw();
   BaseEnemy* CreateEnemy(EnemyType type, const Vector3& pos);
	void Clear();
	void ForEachEnemy(const std::function<void(class BaseEnemy*)>& cb);

	bool ContainsAliveEnemy(const BaseEnemy* enemy) const;

	void SetOnEnemyDeadCallback(const std::function<void(BaseEnemy*)>& cb) { onEnemyDead_ = cb; }
	void SetXPOrbSpawner(const std::function<void(const Vector3&, AbilityId, int)>& spawner) { xpOrbSpawner_ = spawner; }

	void DrawImGui();

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* colliders) { blockColliders_ = colliders; }

	void SetKeepInsideCylinder(const CollisionUtility::Cylinder* cylinder){ keepInsideCylinder_ = cylinder; }

	// 【追加】
	float GetTotalPlayerSpeedMultiplier() const;

private:
	void CheckPlayerProjectileHits(Player* player);

	Object3dCommon* common_ = nullptr;
	Camera* camera_ = nullptr;
	std::list<std::unique_ptr<BaseEnemy>> enemies_;

	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;
    // 敵ごとのドロップテーブルを保持するマップ
	std::unordered_map<int, std::vector<BaseEnemy::DropEntry>> dropTables_;

	const CollisionUtility::Cylinder* keepInsideCylinder_ = nullptr;
  std::function<void(BaseEnemy*)> onEnemyDead_;
  std::function<void(const Vector3&, AbilityId, int)> xpOrbSpawner_;
};