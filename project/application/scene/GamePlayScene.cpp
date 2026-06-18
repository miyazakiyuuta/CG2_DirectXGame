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

	skyCylinder_ = std::make_unique<SkyCylinder>();
	skyCylinder_->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance(), "resources/uvChecker.png");
	skyCylinder_->SetCamera(camera_.get());
	skyCylinder_->GetTransform().scale = { 50.0f, 20.0f, 50.0f };
	skyCylinder_->GetTransform().translate = { 0.0f,  -5.0f,  0.0f };

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());

	effectManager_->FindEffect("Monochrome")->enabled = true;

}

void GamePlayScene::Finalize() {
}

void GamePlayScene::Update() {

	skyCylinder_->Update();

	camera_->Update();
	camera_->TransferToGPU();
	if (Input::GetInstance()->IsTriggerKey(DIK_0)) {
		object3d_->StopAnimation();
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_9)) {
		object3d_->StopAnimation(0.5f);
	}
	if (Input::GetInstance()->IsPushKey(DIK_1)) {
		object3d_->PlayAnimation("walk", true, 0.1f);
	}
	if (Input::GetInstance()->IsTriggerKey(DIK_2)) {
		object3d_->PlayAnimation("sneakWalk", true, 0.1f);
	}

	object3d_->Update();

	DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 5.0f, 10, { 0.0f,0.0f,0.0f,1.0f });

}

void GamePlayScene::Draw() {
	//skybox_->Draw(*camera_);
	skyCylinder_->Draw();

	object3d_->Draw();

	DebugRenderer::GetInstance()->RenderAll(*camera_);
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