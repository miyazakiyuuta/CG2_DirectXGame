#pragma once
#include "3d/Object3dCommon.h"
#include "scene/BaseScene.h"
#include "utility/CollisionUtility.h"

#include "Enemy/Bug/Bug.h"
#include "UI/ResultUI.h"
#include "Reticle.h"
#include "Slug.h"
#include "UI/GameTimer.h"
#include "UI/SpriteNumberText.h"
#include "audio/SoundManager.h"

#include "XPOrb.h"

#include <cstdint>
#include <d3d12.h>
#include <memory>
#include <unordered_map>
#include <vector>

class Camera;
class DebugCamera;
class Object3d;
class Player;
class CameraController;
class Stage;
class StageEditor;
class Skybox;
class DebugGrid;
class EnemyManager;
class PauseMenu;

class GamePlayScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	void DrawImGui() override;

	GamePlayScene();
	~GamePlayScene() override;

private:
	struct EnemySpawnRuntime {
		Vector3 basePosition{};
		int enemyType = 0;
		float respawnIntervalSec = 5.0f;
		float cooldownSec = 0.0f;
		class BaseEnemy* current = nullptr;
	};

	std::vector<EnemySpawnRuntime> enemySpawns_;
	std::unordered_map<class BaseEnemy*, size_t> enemyToSpawnIndex_;
	bool enemiesInitializedFromStage_ = false;
	float enemySpawnYOffset_ = 5.0f;
	void InitializeEnemiesFromStage();
	void UpdateEnemyRespawns(float deltaTime);
	void SpawnEnemyForPoint(size_t idx);
	void OnEnemyDead(class BaseEnemy* e);

	std::unique_ptr<Camera> camera_ = nullptr;
	std::unique_ptr<DebugCamera> debugCamera_;

	std::unique_ptr<Object3d> object3d_;
	// Single static well object placed in the scene
	std::unique_ptr<Object3d> wellObject_;
	std::unique_ptr<Player> player_;
	std::unique_ptr<CameraController> cameraController_;

	std::unique_ptr<Stage> stage_;
	std::unique_ptr<StageEditor> stageEditor_;

	// エネミーを一括管理するマネージャー
	std::unique_ptr<EnemyManager> enemyManager_;
	// ポーズメニュー
	std::unique_ptr<PauseMenu> pauseMenu_;
	bool wasPaused_ = false;

	std::vector<CollisionUtility::OBB> stageBlockColliders_;
	std::vector<CollisionUtility::OBB> breakableBlockColliders_;
	std::vector<CollisionUtility::OBB> waterBlockColliders_;

	// warp cooldown to avoid immediate re-triggering
	int warpCooldownCounter_ = 0;
	int lastWarpId_ = -1;
	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<DebugGrid> debugGrid_;

	CollisionUtility::Cylinder wellCylinder_ = {};

	std::unique_ptr<Reticle> reticle_ = nullptr;
	// XP orb pool
	std::vector<XPOrb> xpOrbs_;

	GameTimer gameTimer_;
	SpriteNumberText timerText_;
	std::unique_ptr<Sprite> timerColonSprite_ = nullptr;

	// BGM用音声データ
	SoundData bgm_ = {};

	// BGMの安全な停止に用いるサウンドハンドル
	SoundManager::SoundHandle bgmHandle_ = SoundManager::InvalidHandle;

	// --- リザルト演出用UI ---
	std::unique_ptr<class ResultUI> resultUI_;
};
