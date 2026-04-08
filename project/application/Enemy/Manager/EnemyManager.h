#pragma once
#include "utility/CollisionUtility.h"
#include <functional>
#include <list>
#include <memory>
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
	void CreateEnemy(EnemyType type, const Vector3& pos);
	void Clear();
	void ForEachEnemy(const std::function<void(class BaseEnemy*)>& cb);

	void SetBlockColliders(const std::vector<CollisionUtility::OBB>* colliders) { blockColliders_ = colliders; }

	// 【追加】
	float GetTotalPlayerSpeedMultiplier() const;

private:
	Object3dCommon* common_ = nullptr;
	Camera* camera_ = nullptr;
	std::list<std::unique_ptr<BaseEnemy>> enemies_;

	const std::vector<CollisionUtility::OBB>* blockColliders_ = nullptr;
};