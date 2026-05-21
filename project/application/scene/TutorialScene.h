#pragma once

#include "scene/BaseScene.h"
#include "utility/CollisionUtility.h"
#include "Tutorial/TutorialDirector.h"

#include <memory>
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
	void SetupTutorialTasks();
	void UpdateTutorial(float deltaTime);
	void UpdateStageColliders();

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

	bool wasPaused_ = false;
};