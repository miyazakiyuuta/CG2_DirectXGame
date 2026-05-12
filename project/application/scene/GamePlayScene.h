#pragma once
#include "3d/Object3dCommon.h"
#include "scene/BaseScene.h"
#include "utility/CollisionUtility.h"

#include "Enemy/Bug/Bug.h"
#include "Slug.h"
#include "Reticle.h"


#include <cstdint>
#include <d3d12.h>
#include <memory>
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
};