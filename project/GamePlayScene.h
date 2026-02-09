#pragma once
#include "BaseScene.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>

#include "engine/audio/SoundManager.h"

class Camera;
class ImGuiManager;
class DebugCamera;

class Object3d;
class ParticleEmitter;
class Sprite;

class GamePlayScene : public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;
	
	void Update() override;
	
	void Draw() override;

private:
	Camera* camera_ = nullptr;
	ImGuiManager* imGuiManager_ = nullptr;
	DebugCamera* debugCamera_;

	SoundData se_;

	Object3d* object3d_;

	Object3d* monsterBall_;

	ParticleEmitter* testParticle_;

	const uint32_t kMaxSprite = 5;
	std::vector<Sprite*> sprites_;

	Sprite* testSprite_;

};

