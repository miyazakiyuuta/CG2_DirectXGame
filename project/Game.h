#pragma once

#include "Framework.h"

#include <d3d12.h>
#include <cstdint>
#include <vector>


#include "engine/audio/SoundManager.h"




class Camera;
class ImGuiManager;
//class SoundManager;
class DebugCamera;

class Object3d;
class ParticleEmitter;
class Sprite;

// ゲーム全体
class Game : public Framework{
public: // メンバ関数
	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 毎フレーム更新
	void Update() override;

	// 描画
	void Draw() override;

private:

	// 基盤システム
	
	
	
	
	
	Camera* camera_ = nullptr;
	ImGuiManager* imGuiManager_ = nullptr;
	DebugCamera* debugCamera_;

	// その他
	SoundData se_;

	Object3d* object3d_;

	Object3d* monsterBall_;

	ParticleEmitter* testParticle_;

	const uint32_t kMaxSprite = 5;
	std::vector<Sprite*> sprites_;

	Sprite* testSprite_;
};

