#pragma once
#include "scene/BaseScene.h"
#include "3d/Object3dCommon.h"
#include "utility/CollisionUtility.h"

#include "Bug.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>

class Camera;
class ImGuiManager;
class DebugCamera;
class Object3d;
class Player;
class CameraController;
class StageEdit;
class DebugGrid;

class GamePlayScene : public BaseScene{
public:
	void Initialize() override;
	void Finalize() override;
	void Update() override;
	void Draw() override;

	GamePlayScene();
	~GamePlayScene() override;

private:
	std::unique_ptr<Camera> camera_ = nullptr;
	std::unique_ptr<ImGuiManager> imGuiManager_ = nullptr;
	std::unique_ptr<DebugCamera> debugCamera_;

	std::unique_ptr<Object3d> object3d_;
	std::unique_ptr<Player> player_;
	std::unique_ptr<CameraController> cameraController_;
	std::unique_ptr<StageEdit> stageEditor_;

	std::unique_ptr<Bug> bug_ = nullptr;
	std::unique_ptr<DebugGrid> debugGrid_;

	std::vector<CollisionUtility::AABB> stageBlockColliders_;
};