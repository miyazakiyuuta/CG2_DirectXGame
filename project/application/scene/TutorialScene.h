#pragma once

#include "scene/BaseScene.h"
#include "utility/CollisionUtility.h"
#include "Tutorial/TutorialDirector.h"
#include "2d/Sprite.h"
#include "math/Vector2.h"

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
		std::string message;
		int requiredScore = 1;
		std::function<int(const TutorialContext&)> scoreDelta;
		float completeWaitSeconds = 0.5f;
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
	void UpdateStageColliders();

	void InitializeTutorialScoreBar();
	void UpdateTutorialScoreBar();
	void DrawTutorialScoreBar();

private:
	std::unique_ptr<Camera> camera_ = nullptr;

	std::unique_ptr<Object3d> wellObject_ = nullptr;
	std::unique_ptr<Player> player_ = nullptr;
	std::unique_ptr<CameraController> cameraController_ = nullptr;
	std::unique_ptr<Stage> stage_ = nullptr;
	std::unique_ptr<Reticle> reticle_ = nullptr;
	std::unique_ptr<DebugGrid> debugGrid_ = nullptr;
	std::unique_ptr<PauseMenu> pauseMenu_ = nullptr;

	CollisionUtility::Cylinder wellCylinder_ = {};

	std::vector<CollisionUtility::OBB> stageBlockColliders_;
	std::vector<CollisionUtility::OBB> breakableBlockColliders_;

	TutorialDirector tutorialDirector_;
	std::vector<TutorialStepDefinition> tutorialStepDefinitions_;

	std::vector<std::unique_ptr<Sprite>> tutorialMessageSprites_;
	std::unique_ptr<Sprite> tutorialFinishedMessageSprite_ = nullptr;

	float tutorialTextDrawScale_ = 0.5f;

	bool wasPaused_ = false;

	std::unique_ptr<Sprite> tutorialScoreBackSprite_ = nullptr;
	std::unique_ptr<Sprite> tutorialScoreFillSprite_ = nullptr;

	Vector2 tutorialScoreBarCenterPos_ = { 0.0f, 0.0f };
	Vector2 tutorialScoreBarSize_ = { 640.0f, 18.0f };
};