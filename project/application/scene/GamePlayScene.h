#pragma once
#include "scene/BaseScene.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>
#include <memory>

#include "audio/SoundManager.h"
#include "3d/Object3dCommon.h"


class Camera;
class ImGuiManager;
class DebugCamera;

class Object3d;
class ParticleEmitter;
class Sprite;
class Player;
class CameraController;
class StageEdit;

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

	SoundData se_;

	std::unique_ptr<Object3d> object3d_;

	std::unique_ptr<Object3d> monsterBall_;

	std::unique_ptr<Object3d> terrain_;

	std::unique_ptr<Player> player_;
	std::unique_ptr<CameraController> cameraController_;
	std::unique_ptr<StageEdit> stageEdit_;

	PointLight pointLight_{};

	SpotLight spotLight_{};

	std::unique_ptr<ParticleEmitter> testParticle_;

	const uint32_t kMaxSprite = 5;
	std::vector<std::unique_ptr<Sprite>> sprites_;

	std::unique_ptr<Sprite> testSprite_;

};

