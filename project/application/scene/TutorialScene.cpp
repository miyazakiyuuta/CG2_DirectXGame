#include "scene/TutorialScene.h"

#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/Camera.h"
#include "3d/ModelManager.h"
#include "3d/Object3d.h"
#include "3d/Object3dCommon.h"
#include "CameraController.h"
#include "Player.h"
#include "Reticle.h"
#include "Stage.h"
#include "StageSerializer.h"
#include "UI/PauseMenu.h"
#include "base/WinApp.h"
#include "debug/DebugGrid.h"
#include "debug/DebugRenderer.h"
#include "io/Input.h"
#include "scene/SceneManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <numbers>

TutorialScene::TutorialScene() = default;
TutorialScene::~TutorialScene() = default;

void TutorialScene::Initialize()
{
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 7.5f, -20.0f });

	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");
	TextureManager::GetInstance()->LoadTexture("resources/circle.png");

	ModelManager::GetInstance()->LoadModel("Cube.obj");
	ModelManager::GetInstance()->LoadModel("tongue/tongue.obj");
	ModelManager::GetInstance()->LoadModel("Frog", "Frog.gltf");
	ModelManager::GetInstance()->LoadModel("well", "well.obj");

	// 井戸の見た目と移動制限円柱
	if (ModelManager::GetInstance()->FindModel("well.obj")) {
		wellObject_ = std::make_unique<Object3d>();
		wellObject_->Initialize(Object3dCommon::GetInstance());
		wellObject_->SetModel("well.obj");
		wellObject_->SetCamera(camera_.get());
		wellObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
		wellObject_->SetScale({ 60.0f, 500.0f, 60.0f });
		wellObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });

		wellCylinder_.center = { 0.0f, 0.0f, 0.0f };
		wellCylinder_.radius = 58.5f;
		wellCylinder_.halfHeight = 1000.0f;
	}

	stage_ = std::make_unique<Stage>(Object3dCommon::GetInstance(), camera_.get());

	auto loadedStage = StageSerializer::LoadFromFile("resources/tutorial_stage.json");
	if (loadedStage) {
		stage_->SetStageData(*loadedStage);
	}

	Vector3 playerStart = { 0.0f, 3.0f, 0.0f };
	if (auto spawn = stage_->GetPlayerSpawnPosition()) {
		playerStart = *spawn;
	}

	player_ = std::make_unique<Player>();
	player_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Frog.gltf", playerStart);
	player_->SetStage(stage_.get());
	player_->SetMovementLimitCylinder(&wellCylinder_);

	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(camera_.get());
	cameraController_->SetTargetOffset({ 0.0f, 1.0f, 0.0f });
	cameraController_->SetDistance(25.0f);
	cameraController_->SetHeight(1.5f);
	cameraController_->SetYawSpeed(0.03f);
	cameraController_->SetPitchSpeed(0.02f);
	cameraController_->SetObstacleColliders(&stageBlockColliders_);
	cameraController_->SetObstacleCylinder(&wellCylinder_);
	cameraController_->SetKeepInsideCylinder(&wellCylinder_);

	player_->SetCameraController(cameraController_.get());

	reticle_ = std::make_unique<Reticle>();
	reticle_->Initialize(
		SpriteCommon::GetInstance(),
		camera_.get(),
		cameraController_.get(),
		player_.get(),
		&stageBlockColliders_
	);

	pauseMenu_ = std::make_unique<PauseMenu>();
	pauseMenu_->Initialize(SpriteCommon::GetInstance(), cameraController_.get());

	debugGrid_ = std::make_unique<DebugGrid>();
	debugGrid_->Initialize(DirectXCommon::GetInstance());

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());

	Object3dCommon::GetInstance()->SetPointLight({
		{ 1.0f, 1.0f, 1.0f, 1.0f },
		{ 0.0f, -0.1f, 0.0f },
		1.0f,
		20000.0f,
		0.0f
		});

	SetupTutorialTasks();
	tutorialDirector_.Start();

#ifndef USE_IMGUI
	while (ShowCursor(FALSE) >= 0) {}
	HWND hwnd = WinApp::GetInstance()->GetHwnd();
	RECT rect;
	GetClientRect(hwnd, &rect);
	ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.left));
	ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.right));
	ClipCursor(&rect);
#endif
}

void TutorialScene::Finalize()
{
#ifndef USE_IMGUI
	ShowCursor(TRUE);
	ClipCursor(nullptr);
#endif
}

void TutorialScene::SetupTutorialTasks()
{
	tutorialDirector_.Clear();

	tutorialDirector_.AddTask({
		"移動",
		"WASD または左スティックで移動してみよう",
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return false;
			}

			return
				ctx.input->IsPushKey(DIK_W) ||
				ctx.input->IsPushKey(DIK_A) ||
				ctx.input->IsPushKey(DIK_S) ||
				ctx.input->IsPushKey(DIK_D);
		},
		nullptr,
		0.5f
		});

	tutorialDirector_.AddTask({
		"ジャンプ",
		"Space でジャンプしてみよう",
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return false;
			}

			return ctx.input->IsTriggerKey(DIK_SPACE);
		},
		nullptr,
		0.5f
		});

	tutorialDirector_.AddTask({
		"エイム",
		"右クリックでエイムしてみよう",
		[](const TutorialContext& ctx) {
			if (!ctx.cameraController) {
				return false;
			}

			return ctx.cameraController->IsAimMode();
		},
		nullptr,
		0.5f
		});

	tutorialDirector_.AddTask({
		"舌",
		"左クリックで舌を伸ばしてみよう",
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return false;
			}

			return ctx.input->IsTriggerMouse(0);
		},
		nullptr,
		0.5f
		});
}

void TutorialScene::UpdateTutorial(float deltaTime)
{
	TutorialContext context;
	context.player = player_.get();
	context.cameraController = cameraController_.get();
	context.input = Input::GetInstance();
	context.deltaTime = deltaTime;

	tutorialDirector_.Update(context);
}

void TutorialScene::UpdateStageColliders()
{
	if (!stage_) {
		return;
	}

	stageBlockColliders_ = stage_->GetBlockOBBs();

	breakableBlockColliders_.clear();

	if (player_) {
		player_->SetBlockColliders(&stageBlockColliders_);
		player_->SetBreakableBlockColliders(&breakableBlockColliders_);
	}

	if (cameraController_) {
		cameraController_->SetObstacleColliders(&stageBlockColliders_);
	}
}

void TutorialScene::Update()
{
	const float deltaTime = 1.0f / 60.0f;

	if (pauseMenu_) {
		pauseMenu_->Update();

		if (pauseMenu_->IsTitleRequested()) {
			SceneManager::GetInstance()->ChangeScene("TITLE");
			return;
		}
	}

	const bool isPaused = pauseMenu_ && pauseMenu_->IsPaused();

#ifndef USE_IMGUI
	if (isPaused != wasPaused_) {
		if (isPaused) {
			ShowCursor(TRUE);
			ClipCursor(nullptr);
		}
		else {
			ShowCursor(FALSE);
			HWND hwnd = WinApp::GetInstance()->GetHwnd();
			RECT rect;
			GetClientRect(hwnd, &rect);
			ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.left));
			ClientToScreen(hwnd, reinterpret_cast<POINT*>(&rect.right));
			ClipCursor(&rect);
		}

		wasPaused_ = isPaused;
	}
#endif

	if (!isPaused) {
		if (stage_) {
			stage_->Update(deltaTime);
		}

		UpdateStageColliders();

		if (reticle_) {
			reticle_->Update();

			if (player_) {
				if (reticle_->HasAimTargetPoint()) {
					player_->SetAimTargetPoint(reticle_->GetAimTargetPoint());
				}
				else {
					player_->ClearAimTargetPoint();
				}
			}
		}

		if (cameraController_ && player_) {
			cameraController_->Update(player_->GetPosition());
		}

		if (player_) {
			player_->Update();
			player_->UpdateTransparencyByCamera(camera_->GetTranslate());
		}

		UpdateTutorial(deltaTime);
	}

	if (camera_) {
		camera_->Update();
		camera_->TransferToGPU();
	}

	if (wellObject_) {
		wellObject_->Update();
	}

	DebugRenderer::GetInstance()->AddGrid(
		{ 0.0f, 0.0f, 0.0f },
		5.0f,
		10,
		{ 0.0f, 0.0f, 0.0f, 1.0f }
	);
}

void TutorialScene::Draw()
{
	if (wellObject_) {
		wellObject_->Draw();
	}

	if (stage_) {
		stage_->Draw();
	}

	if (player_) {
		player_->Draw();
	}

	if (debugGrid_) {
		debugGrid_->Draw(*camera_);
	}

	SpriteCommon::GetInstance()->CommonDrawSetting();

	if (reticle_) {
		reticle_->Draw();
	}

	if (player_) {
		player_->DrawUI();
	}

	if (pauseMenu_) {
		pauseMenu_->Draw();
	}

	DebugRenderer::GetInstance()->RenderAll(*camera_);
}

void TutorialScene::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Tutorial");

	ImGui::TextUnformatted(tutorialDirector_.GetCurrentTitle().c_str());
	ImGui::Separator();
	ImGui::TextWrapped("%s", tutorialDirector_.GetCurrentMessage().c_str());
	ImGui::Text("Task Index: %d", tutorialDirector_.GetCurrentIndex());

	if (tutorialDirector_.IsFinished()) {
		ImGui::TextUnformatted("Tutorial Finished");
	}

	ImGui::End();

	if (player_) {
		player_->DrawImGui();
	}

	if (cameraController_) {
		cameraController_->DrawImGui();
	}
#endif
}