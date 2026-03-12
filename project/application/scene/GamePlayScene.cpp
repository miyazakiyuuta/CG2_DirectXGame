#include "scene/GamePlayScene.h"

#include "base/ImGuiManager.h"
#include "io/Input.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "3d/ModelManager.h"
#include "3d/DebugCamera.h"
#include "3d/Object3dCommon.h"
#include "effect/ParticleManager.h"
#include "effect/RingManager.h"
#include "effect/CylinderManager.h"
#include "audio/SoundManager.h"

#include "2d/Sprite.h"
#include "3d/Object3d.h"
#include "effect/ParticleEmitter.h"
#include "debug/DebugGrid.h"

#include <imgui.h>
#include <numbers>

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,7.5f,-20.0f });

	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());

	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	ParticleManager::GetInstance()->SetCamera(camera_.get());

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	// TextureManager からテクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("AnimatedCube.gltf");
	ModelManager::GetInstance()->LoadModel("human", "walk.gltf");
	ModelManager::GetInstance()->LoadModel("human", "sneakWalk.gltf");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("sneakWalk.gltf");
	object3d_->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d_->SetCamera(camera_.get());

	debugGrid_ = std::make_unique<DebugGrid>();
	debugGrid_->Initialize(DirectXCommon::GetInstance());
}

void GamePlayScene::Finalize() {
}

void GamePlayScene::Update() {
	imGuiManager_->Begin();
#ifdef USE_IMGUI
	// デモウィンドウ(使い方紹介)
	ImGui::ShowDemoWindow();

	Vector3 rotate = object3d_->GetRotate();

	ImGui::Begin("Window");

	camera_->DrawImGui();
	if (ImGui::TreeNode("object3d")) {
		ImGui::DragFloat3("rotate", &rotate.x, 0.01f);
		ImGui::TreePop();
	}

	ImGui::End();

	imGuiManager_->End();

	object3d_->SetRotate(rotate);
#endif
	camera_->Update();
	camera_->TransferToGPU();

	object3d_->Update();

	
}

void GamePlayScene::Draw() {
	SrvManager::GetInstance()->PreDraw();
	object3d_->Draw();

	debugGrid_->Draw(*camera_);
}

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;