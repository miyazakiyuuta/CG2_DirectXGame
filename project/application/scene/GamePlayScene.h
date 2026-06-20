#pragma once
#include "scene/BaseScene.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>
#include "math/Vector3.h"

class Camera;
class DebugCamera;
class Object3d;
class Skybox;
class DebugGrid;
class GPUParticleEmitter;
class SkyCylinder;
class ImplosionEffect;
class ExplosionEffect;
class FireEffect;

class GamePlayScene : public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;
	
	void Update(float deltaTime) override;
	
	void Draw() override;

	void DrawImGui() override;

	GamePlayScene();

	~GamePlayScene() override;

private:
	std::unique_ptr<Camera> camera_ = nullptr;
	std::unique_ptr<DebugCamera> debugCamera_;

	std::unique_ptr<GPUParticleEmitter> gpuParticleEmitter_;

	std::unique_ptr<ImplosionEffect> implosion_;
	std::unique_ptr<ExplosionEffect> explosion_;
	std::unique_ptr<FireEffect> fire_;

	std::vector<Vector3> torchPositions_;
};

