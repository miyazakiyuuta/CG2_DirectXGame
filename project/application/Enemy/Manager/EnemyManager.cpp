#include "EnemyManager.h"
#include "../Core/BaseEnemy.h"
#include "../Types/ChasingEnemy.h"
#include "../Types/ClusterSlime.h"
#include "3d/Object3d.h"
#include <algorithm>

EnemyManager::EnemyManager() = default;
EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize(Object3dCommon* common, Camera* camera) {
	common_ = common;
	camera_ = camera;
	enemies_.clear();
}

void EnemyManager::Update(float deltaTime, const Vector3& playerPos) {
	for (auto& enemy : enemies_) {
		if (enemy) {
			enemy->SetBlockColliders(blockColliders_);
			enemy->Update(deltaTime, playerPos);
		}
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
	switch (type) {
	case EnemyType::Chasing:
		e = std::make_unique<ChasingEnemy>();
		break;
	case EnemyType::ClusterSlime:
		e = std::make_unique<ClusterSlime>();
		break;
	default:
		break;
	}
	if (e) {
		e->Initialize(common_, camera_, pos);
		e->SetBlockColliders(blockColliders_);
		enemies_.push_back(std::move(e));
	}
}

float EnemyManager::GetTotalPlayerSpeedMultiplier() const {
	float finalMultiplier = 1.0f;
	for (const auto& enemy : enemies_) {
		if (enemy)
			finalMultiplier = (std::min)(finalMultiplier, enemy->GetPlayerSpeedMultiplier());
	}
	return finalMultiplier;
}

void EnemyManager::Clear() { enemies_.clear(); }

void EnemyManager::ForEachEnemy(const std::function<void(BaseEnemy*)>& cb) {
	for (auto& e : enemies_)
		if (e && !e->IsDead())
			cb(e.get());
}