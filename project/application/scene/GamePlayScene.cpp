#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/RingManager.h"
#include "effect/CylinderManager.h"
#include "effect/GPUParticleEmitter.h"
#include "audio/SoundManager.h"

#include "2d/Sprite.h"
#include "3d/Object3d.h"
#include "effect/ParticleEmitter.h"
#include "3d/Skybox.h"
#include "debug/DebugGrid.h"
#include "debug/DebugRenderer.h"

#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,7.5f,-20.0f });

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

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("human_re.gltf");
	//object3d_->SetModel("Frog.gltf");
	object3d_->SetCamera(camera_.get());
	object3d_->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d_->SetColor({ 0.5f,0.5f,0.5f,1.0f });
	object3d_->SetUseEnvironmentMap(true); // 環境マップ

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

	debugGrid_ = std::make_unique<DebugGrid>();
	debugGrid_->Initialize(DirectXCommon::GetInstance());
	debugGrid_->SetPosition({ 0.0f,0.0f,0.0f });
	debugGrid_->SetColor({ 0.0f,0.0f,0.0f,1.0f });

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());

	gpuParticleEmitter_ = std::make_unique<GPUParticleEmitter>();
	gpuParticleEmitter_->Initialize();
	uint32_t texIndex = TextureManager::GetInstance()->GetSrvIndex("resources/circle.png");
	gpuParticleEmitter_->SetTexture(texIndex);

}

void GamePlayScene::Finalize() {
}

void GamePlayScene::Update() {

	camera_->Update();
	camera_->TransferToGPU();
	if (Input::GetInstance()->IsTriggerKey(DIK_0)) {
		object3d_->StopAnimation();
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_9)) {
		object3d_->PauseSwitchingAnimation();
	}
	if (Input::GetInstance()->IsPushKey(DIK_1)) {
		object3d_->PlayAnimation("walk", false, 1.0f);
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_2)) {
		object3d_->PlayAnimation("sneakWalk", true, 1.0f);
	}

	object3d_->Update();

	gpuParticleEmitter_->Update(1.0f / 60.0f);

	DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 5.0f, 10, { 0.0f,0.0f,0.0f,1.0f });

}

void GamePlayScene::Draw() {
	//skybox_->Draw(*camera_);

	object3d_->Draw();

	DebugRenderer::GetInstance()->RenderAll(*camera_);

	gpuParticleEmitter_->Draw(camera_.get());
}

void GamePlayScene::DrawImGui() {
#ifdef USE_IMGUI

	Vector3 object3dPos = object3d_->GetTranslate();

	ImGui::Begin("GamePlayScene_Object");
	camera_->DrawImGui();
	if (ImGui::DragFloat3("object3d_->pos", &object3dPos.x, 0.01f)) {
		object3d_->SetTranslate(object3dPos);
	}
	ImGui::End();

#endif
}

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;