#pragma once
#include "scene/BaseScene.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>

class Camera;
class DebugCamera;
class Object3d;
class Skybox;
class DebugGrid;
class GPUParticleEmitter;
class CylinderSkybox;

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

	std::unique_ptr<Skybox> skybox_;

	std::unique_ptr<GPUParticleEmitter> gpuParticleEmitter_;

	std::unique_ptr<CylinderSkybox> cylinderSkybox_;
};

