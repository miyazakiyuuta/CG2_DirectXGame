#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "2d/TextureManager.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/GPUParticleEmitter.h"
#include "3d/Object3d.h"
#include "3d/Skybox.h"
#include "3d/SkyCylinder.h"
#include "debug/DebugRenderer.h"
#include "effect/EffectManager.h"
#include "effect/RingManager.h"
#include "effect/ImplosionEffect.h"
#include "effect/ExplosionEffect.h"
#include "effect/FireEffect.h"


#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	//camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	//camera_->SetTranslate({ 0.0f,7.5f,-20.0f });
	camera_->SetRotate({ std::numbers::pi_v<float> / 40.0f, 0.0f, 0.0f }); // ~4.5°の軽い見下ろし
	camera_->SetTranslate({ 0.0f, 1.6f, -9.0f });

	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	ParticleManager::GetInstance()->SetCamera(camera_.get());

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// TextureManager からテクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");
	TextureManager::GetInstance()->LoadTexture("resources/circle.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("human", "sneakWalk.gltf");
	ModelManager::GetInstance()->LoadModel("human", "human_re.gltf");
	ModelManager::GetInstance()->LoadModel("Frog", "Frog.gltf");

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	RingManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	RingManager::GetInstance()->SetCamera(camera_.get());

	implosion_ = std::make_unique<ImplosionEffect>();
	implosion_->Initialize();

	explosion_ = std::make_unique<ExplosionEffect>();
	explosion_->Initialize();
	explosion_->loop_ = false;            // ショーケースとして自動ループ
	//explosion_->Trigger({ 0.0f, 2.0f, 0.0f });

	fire_ = std::make_unique<FireEffect>();
	fire_->Initialize();
	//fire_->Ignite({ 0.0f, 0.0f, 0.0f });

	// 参道: Z方向に等間隔、左右対称に篝火を並べる
	{
		const int   pairs = 10;     // 何対置くか
		const float spacingZ = 4.0f;  // 前後の間隔
		const float halfWidth = 3.0f;  // 道の半幅(左右の距離)
		const float startZ = -9.0f; // 手前端
		const float baseY = 0.0f;

		torchPositions_.clear();
		torchPositions_.reserve(pairs * 2);
		for (int i = 0; i < pairs; ++i) {
			float z = startZ + spacingZ * i;
			torchPositions_.push_back({ -halfWidth, baseY, z }); // 左
			torchPositions_.push_back({ +halfWidth, baseY, z }); // 右
		}
	}
	//fire_->SetPositions(torchPositions_);

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());

	effectManager_->FindEffect("Monochrome")->enabled = false;
	effectManager_->FindEffect("RadialBlur")->enabled = true;

}

void GamePlayScene::Finalize() {
}

void GamePlayScene::Update(float deltaTime) {

	camera_->Update();
	camera_->TransferToGPU();

	implosion_->Update(deltaTime);
	explosion_->Update(deltaTime);
	fire_->Update(deltaTime);
	ParticleManager::GetInstance()->Update(deltaTime);
	RingManager::GetInstance()->Update(deltaTime);

	if (Input::GetInstance()->IsTriggerKey(DIK_1)) {
		implosion_->Trigger({ 0.0f, 0.0f, 10.0f });
	}

	if (Input::GetInstance()->IsTriggerKey(DIK_2)) {
		explosion_->Trigger({ 0.0f, 0.0f, 10.0f });
	}

	if (Input::GetInstance()->IsTriggerKey(DIK_3)) {
		if (fire_->IsBurning()) {
			fire_->Extinguish();
		} else {
			fire_->SetPositions(torchPositions_);
		}
	}

	//DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 10.0f, 20, { 0.5f,0.5f,0.5f,1.0f });

}

void GamePlayScene::Draw() {

	ParticleManager::GetInstance()->Draw();
	RingManager::GetInstance()->Draw();

	DebugRenderer::GetInstance()->RenderAll(*camera_);
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	ImGui::Begin("GamePlayScene_Object");
	camera_->DrawImGui();
	ImGui::End();

#endif
}

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;