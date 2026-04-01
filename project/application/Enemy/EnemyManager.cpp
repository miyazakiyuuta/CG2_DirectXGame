#include "EnemyManager.h"
#include "3d/Object3d.h"
#include "BaseEnemy.h"
#include "ChasingEnemy.h"
#include "ShootingEnemy.h"
#include "SentinelEnemy.h"

EnemyManager::EnemyManager() = default;
EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize(Object3dCommon* common, Camera* camera) {
	common_ = common;
	camera_ = camera;
	enemies_.clear();
}

void EnemyManager::Update(float deltaTime, const Vector3& playerPos) {
	for (auto& enemy : enemies_) {
		if (enemy)
			enemy->Update(deltaTime, playerPos);
	}
	enemies_.remove_if([](const std::unique_ptr<BaseEnemy>& e) { return !e || e->IsDead(); });
}

void EnemyManager::Draw() {
	for (auto& e : enemies_)
		if (e)
			e->Draw();
}

void EnemyManager::CreateEnemy(EnemyType type, const Vector3& pos) {
	if (!common_)
		return;
	std::unique_ptr<BaseEnemy> e;

	if (type == EnemyType::Chasing) {
		e = std::make_unique<ChasingEnemy>();
	} else if (type == EnemyType::Shooting) {
		e = std::make_unique<ShootingEnemy>();
	} else if (type == EnemyType::Sentinel) {
		e = std::make_unique<SentinelEnemy>(); // 正しく生成するように修正
	}

	if (e) {
		e->Initialize(common_, camera_, pos);
		enemies_.push_back(std::move(e));
	}
}

void EnemyManager::Clear() { enemies_.clear(); }