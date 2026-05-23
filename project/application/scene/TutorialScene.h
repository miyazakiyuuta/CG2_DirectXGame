#pragma once

#include "scene/BaseScene.h"
#include "utility/CollisionUtility.h"
#include "Tutorial/TutorialDirector.h"
#include "2d/Sprite.h"
#include "math/Vector2.h"
#include "XPOrb.h"
#include "StageTypes.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Camera;
class Object3d;
class Player;
class CameraController;
class Stage;
class Reticle;
class DebugGrid;
class PauseMenu;
class EnemyManager;

class TutorialScene : public BaseScene {
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;
	void DrawImGui() override;

	TutorialScene();
	~TutorialScene() override;

private:
	struct TutorialStepDefinition {
		std::string title;

		std::string keyboardMessage;
		std::string gamepadMessage;

		int requiredScore = 1;
		std::function<int(const TutorialContext&)> scoreDelta;

		std::function<void()> onEnter;
		std::function<void()> onExit;

		float completeWaitSeconds = 0.5f;
	};

	enum class TutorialInputDevice {
		KeyboardMouse,
		Gamepad,
	};

private:
	void BuildTutorialStepDefinitions();
	void SetupTutorialTasksFromDefinitions();
	void GenerateTutorialMessageSprites();

	std::unique_ptr<Sprite> CreateTutorialMessageSprite(
		const std::string& textUtf8,
		const std::string& outputPath
	);

	Sprite* GetCurrentTutorialMessageSprite() const;

	void UpdateTutorial(float deltaTime);
	void UpdateTutorialFrameEvents();
	void UpdateStageColliders();

	void InitializeTutorialScoreBar();
	void UpdateTutorialScoreBar();
	void DrawTutorialScoreBar();

	void UpdateLastInputDevice();
	bool HasKeyboardMouseTutorialInput() const;
	bool HasGamepadTutorialInput() const;

	// フェーズ専用 StageObject
	StageObject MakeTutorialBlock(
		BlockID blockId,
		const Vector3& position,
		const Vector3& scale,
		const Vector4& color,
		int hp = 0
	) const;

	int AddTutorialStageObject(const StageObject& object);
	void ClearTutorialPhaseObjects();
	void ClearTutorialEnemies();
	void ClearTutorialXP();
	void ClearTutorialPhaseRuntime(bool clearXP = true);

	void SpawnTutorialClingBlocks();
	void SpawnTutorialBreakableBlocks();
	void SpawnTutorialWarpBlock();
	void SpawnTutorialWeakEnemy();
	void SpawnTutorialSentinelHook();

	bool IsTutorialStageObjectAlive(int id) const;
	int CountAliveTutorialObjects(BlockID blockId) const;

	// チュートリアル専用ランタイム
	void UpdateTutorialEnemies(float deltaTime);
	void UpdateTutorialXPOrbs(float deltaTime);
	void WriteTutorialXPOrbInstances();
	void UpdateTutorialWarp();

private:
	std::unique_ptr<Camera> camera_ = nullptr;

	std::unique_ptr<Object3d> wellObject_ = nullptr;
	std::unique_ptr<Player> player_ = nullptr;
	std::unique_ptr<CameraController> cameraController_ = nullptr;
	std::unique_ptr<Stage> stage_ = nullptr;
	std::unique_ptr<Reticle> reticle_ = nullptr;
	std::unique_ptr<DebugGrid> debugGrid_ = nullptr;
	std::unique_ptr<PauseMenu> pauseMenu_ = nullptr;

	std::unique_ptr<EnemyManager> enemyManager_ = nullptr;
	std::vector<XPOrb> xpOrbs_;

	CollisionUtility::Cylinder wellCylinder_ = {};

	std::vector<CollisionUtility::OBB> stageBlockColliders_;
	std::vector<CollisionUtility::OBB> breakableBlockColliders_;

	TutorialDirector tutorialDirector_;
	std::vector<TutorialStepDefinition> tutorialStepDefinitions_;

	TutorialInputDevice lastInputDevice_ = TutorialInputDevice::KeyboardMouse;

	std::vector<std::unique_ptr<Sprite>> tutorialKeyboardMessageSprites_;
	std::vector<std::unique_ptr<Sprite>> tutorialGamepadMessageSprites_;

	std::unique_ptr<Sprite> tutorialKeyboardFinishedMessageSprite_ = nullptr;
	std::unique_ptr<Sprite> tutorialGamepadFinishedMessageSprite_ = nullptr;

	float tutorialTextDrawScale_ = 0.5f;

	bool wasPaused_ = false;

	std::unique_ptr<Sprite> tutorialScoreBackSprite_ = nullptr;
	std::unique_ptr<Sprite> tutorialScoreFillSprite_ = nullptr;

	Vector2 tutorialScoreBarCenterPos_ = { 0.0f, 0.0f };
	Vector2 tutorialScoreBarSize_ = { 640.0f, 18.0f };

	bool tutorialPrevTongueWasIdle_ = true;
	bool tutorialTongueShotStartedThisFrame_ = false;

	std::vector<int> tutorialPhaseObjectIds_;
	int tutorialNextObjectId_ = 900000;

	int tutorialEnemyKilledCount_ = 0;
	int tutorialXPCollectedCount_ = 0;
	int tutorialWarpUsedCount_ = 0;

	int warpCooldownCounter_ = 0;
	int lastWarpId_ = -1;

	float tutorialWallClingPrevGauge_ = 0.0f;
	float tutorialWallClingConsumedTotal_ = 0.0f;
	float tutorialWallClingConsumeRequired_ = 10.0f;
	float tutorialWallClingConsumeRemainder_ = 0.0f;

	// 0.1 スタミナ消費 = 1 スコアにする
	float tutorialWallClingScoreScale_ = 1.0f;
};