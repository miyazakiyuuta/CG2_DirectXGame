#include "EnemyManager.h"
#include "../Core/BaseEnemy.h"
#include "../Types/ChasingEnemy.h"
#include "../Types/ClusterSlime.h"
#include "../Types/ProminenceSensor.h"
#include "../Types/ShootingEnemy.h"   
#include "../Types/SentinelEnemy.h"   
#include "../Types/PhaseGhost.h"
#include "../../Player.h"
#include "3d/Object3d.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <../externals/nlohmann/json.hpp>
#include <iostream>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

EnemyManager::EnemyManager() = default;
EnemyManager::~EnemyManager() = default;

void EnemyManager::Initialize(Object3dCommon* common, Camera* camera) {
	common_ = common;
	camera_ = camera;
	enemies_.clear();

    // resources/enemy_drops.json から敵のドロップテーブルを読み込もうとする
	try {
		const std::string path = "resources/enemy_drops.json";
		if (std::filesystem::exists(path)) {
			std::ifstream ifs(path);
			if (ifs.is_open()) {
				nlohmann::json j;
				ifs >> j;
               // 期待されるフォーマット: [{ "enemyType": int, "drops": [ {"ability":"Name","weight":1.0,"min":1,"max":1,"chance":1.0}, ... ] }, ...]
				for (const auto& item : j) {
					if (!item.contains("enemyType"))
						continue;
					int et = item["enemyType"].get<int>();
					std::vector<BaseEnemy::DropEntry> table;
					if (item.contains("drops")) {
						for (const auto& d : item["drops"]) {
                        BaseEnemy::DropEntry e;
						if (d.contains("ability")) {
							std::string a = d["ability"].get<std::string>();
							e.ability = AbilityIdFromString(a);
						}
							if (d.contains("weight")) e.weight = d["weight"].get<float>();
							if (d.contains("min")) e.minAmount = d["min"].get<int>();
							if (d.contains("max")) e.maxAmount = d["max"].get<int>();
							if (d.contains("chance")) e.chance = d["chance"].get<float>();
							table.push_back(e);
						}
					}
					dropTables_[et] = table;
				}
			}
		}
    } catch (...) {
		// パースエラーは無視する
		std::cerr << "Failed to load enemy_drops.json\n";
	}
}

void EnemyManager::Update(float deltaTime, Player* player) {
	if (!player)
		return;
	Vector3 playerPos = player->GetPosition();

    for (auto& enemy : enemies_) {
		if (enemy) {
			enemy->SetBlockColliders(blockColliders_);
			enemy->SetPlayer(player); // 【追加】情報を渡す
			enemy->Update(deltaTime, playerPos);
		}
	}
    // 死亡した敵を収集し、メインリストから切り離してからドロップを処理する
	std::vector<std::unique_ptr<BaseEnemy>> deadEnemies;
	for (auto it = enemies_.begin(); it != enemies_.end();) {
        if (*it && (*it)->IsDead()) {
			// 切り出す
			deadEnemies.push_back(std::move(*it));
			it = enemies_.erase(it);
		} else {
			++it;
		}
	}

    // 切り出した死体を安全に処理する（deadEnemies が所有）
	for (auto &de : deadEnemies) {
		if (de) {
            // プレイヤーに参照をクリアするよう通知
			player->NotifyEnemyDead(de.get());
            // ドロップをエンキュー（安全なタイミングでプレイヤーに適用）
			de->DistributeDrops();
		}
	}
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

	// 【修正】すべての EnemyType を生成できるように switch 文を完成させる
	switch (type) {
	case EnemyType::Chasing:
		e = std::make_unique<ChasingEnemy>();
		break;
	case EnemyType::Shooting:
		e = std::make_unique<ShootingEnemy>();
		break;
	case EnemyType::Sentinel:
		e = std::make_unique<SentinelEnemy>();
		break;
	case EnemyType::ClusterSlime:
		e = std::make_unique<ClusterSlime>();
		break;
	case EnemyType::ProminenceSensor:
		e = std::make_unique<ProminenceSensor>();
		break;
	case EnemyType::PhaseGhost: // 【追加】
		e = std::make_unique<PhaseGhost>();
		break;
	}

	if (e) {
		e->Initialize(common_, camera_, pos);
		e->SetBlockColliders(blockColliders_);
        // 該当タイプのドロップテーブルがあれば注入する
		int typeInt = static_cast<int>(type);
		auto it = dropTables_.find(typeInt);
		if (it != dropTables_.end()) {
			e->SetDropTable(it->second);
		}
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

void EnemyManager::DrawImGui() {
#ifdef USE_IMGUI
	ImGui::Begin("Enemy Drop Tables");
	if (dropTables_.empty()) {
		ImGui::Text("No drop tables loaded.");
	} else {
		for (const auto &kv : dropTables_) {
			int et = kv.first;
			const auto &table = kv.second;
			char title[64];
			snprintf(title, sizeof(title), "EnemyType %d", et);
			if (ImGui::TreeNode(title)) {
				if (table.empty()) {
					ImGui::Text("(no entries)");
				} else {
                    for (const auto &e : table) {
						ImGui::BulletText("ability: %s weight: %.2f chance: %.2f min:%d max:%d",
							AbilityIdToString(e.ability), e.weight, e.chance, e.minAmount, e.maxAmount);
						}
				}
				ImGui::TreePop();
			}
		}
	}
	ImGui::End();
#endif
}