#pragma once
#include "scene/BaseScene.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>

class Camera;
class ImGuiManager;
class DebugCamera;
class Object3d;
class DebugGrid;

class GamePlayScene : public BaseScene {
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

	std::unique_ptr<DebugGrid> debugGrid_;
};

