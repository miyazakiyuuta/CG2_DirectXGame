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
#include "UI/RuntimeTextTextureGenerator.h"
#include "base/WinApp.h"
#include "debug/DebugGrid.h"
#include "debug/DebugRenderer.h"
#include "io/Input.h"
#include "scene/SceneManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <numbers>
#include <string>

namespace {

	std::string ToUtf8String(const char* text)
	{
		return text ? std::string(text) : std::string();
	}

#if defined(__cpp_char8_t)
	std::string ToUtf8String(const char8_t* text)
	{
		return text ? std::string(reinterpret_cast<const char*>(text)) : std::string();
	}
#endif

} // namespace

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

	// 1. チュートリアルで使うタスクと文字列をまとめて作る
	// 2. 使う文字列を先に全部PNG化してSprite化する
	// 3. そのあとタスク進行を開始する
	BuildTutorialStepDefinitions();
	GenerateTutorialMessageSprites();
	SetupTutorialTasksFromDefinitions();
	tutorialDirector_.Start();

	InitializeTutorialScoreBar();
	UpdateTutorialScoreBar();

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

void TutorialScene::BuildTutorialStepDefinitions()
{
	tutorialStepDefinitions_.clear();

	tutorialStepDefinitions_.push_back({
		"移動",
		ToUtf8String(u8"WASDで1秒間移動してみよう"),
		60,
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return 0;
			}

			const bool moving =
				ctx.input->IsPushKey(DIK_W) ||
				ctx.input->IsPushKey(DIK_A) ||
				ctx.input->IsPushKey(DIK_S) ||
				ctx.input->IsPushKey(DIK_D);

			return moving ? 1 : 0;
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"ジャンプ",
		ToUtf8String(u8"長押しでジャンプを溜めよう"),
		45,
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return 0;
			}

			// Space を押している間だけゲージが進む。
			// 45なら約0.75秒ぶん。
			return ctx.input->IsPushKey(DIK_SPACE) ? 1 : 0;
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"エイム",
		ToUtf8String(u8"右クリックで1秒間エイムしてみよう"),
		60,
		[](const TutorialContext& ctx) {
			if (!ctx.cameraController) {
				return 0;
			}

			return ctx.cameraController->IsAimMode() ? 1 : 0;
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"舌",
		ToUtf8String(u8"左クリックで舌を3回伸ばしてみよう"),
		3,
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return 0;
			}

			return ctx.input->IsTriggerMouse(0) ? 1 : 0;
		},
		0.5f
		});
}

void TutorialScene::SetupTutorialTasksFromDefinitions()
{
	tutorialDirector_.Clear();

	for (const auto& def : tutorialStepDefinitions_) {
		TutorialTask task;
		task.title = def.title;
		task.message = def.message;
		task.requiredScore = def.requiredScore;
		task.scoreDelta = def.scoreDelta;
		task.onEnter = nullptr;
		task.completeWaitSeconds = def.completeWaitSeconds;

		tutorialDirector_.AddTask(task);
	}
}

void TutorialScene::GenerateTutorialMessageSprites()
{
	tutorialMessageSprites_.clear();
	tutorialMessageSprites_.reserve(tutorialStepDefinitions_.size());

	for (size_t i = 0; i < tutorialStepDefinitions_.size(); ++i) {
		std::string outputPath =
			"resources/generated_ui/tutorial_message_" + std::to_string(i) + ".png";

		auto sprite = CreateTutorialMessageSprite(
			tutorialStepDefinitions_[i].message,
			outputPath
		);

		tutorialMessageSprites_.push_back(std::move(sprite));
	}

	tutorialFinishedMessageSprite_ = CreateTutorialMessageSprite(
		ToUtf8String(u8"         チュートリアル完了！\n ESCメニューからタイトルに戻れます"),
		"resources/generated_ui/tutorial_message_finished.png"
	);
}

std::unique_ptr<Sprite> TutorialScene::CreateTutorialMessageSprite(
	const std::string& textUtf8,
	const std::string& outputPath
) {
	RuntimeTextTextureGenerator::GenerateDesc textDesc;
	textDesc.textUtf8 = textUtf8;
	textDesc.fontFilePath = "resources/fonts/KiwiMaru-Medium.ttf";
	textDesc.outputFilePath = outputPath;

	// 高解像度で生成して、Sprite表示時に縮小する
	textDesc.fontPixelSize = 120;
	textDesc.paddingX = 48;
	textDesc.paddingY = 28;
	textDesc.textColor = { 1.0f, 1.0f, 1.0f, 1.0f };
	textDesc.shadowColor = { 0.0f, 0.0f, 0.0f, 0.65f };
	textDesc.shadowOffsetX = 6;
	textDesc.shadowOffsetY = 6;

	if (!RuntimeTextTextureGenerator::GeneratePng(textDesc)) {
		return nullptr;
	}

	TextureManager::GetInstance()->LoadTexture(outputPath);

	const DirectX::TexMetadata& meta =
		TextureManager::GetInstance()->GetMetaData(outputPath);

	auto sprite = std::make_unique<Sprite>();
	sprite->Initialize(SpriteCommon::GetInstance(), outputPath);
	sprite->SetAnchorPoint({ 0.5f, 0.0f });
	sprite->SetPos({
		static_cast<float>(WinApp::kClientWidth) * 0.5f,
		48.0f
		});
	sprite->SetSize({
		static_cast<float>(meta.width) * tutorialTextDrawScale_,
		static_cast<float>(meta.height) * tutorialTextDrawScale_
		});
	sprite->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
	sprite->Update();

	return sprite;
}

Sprite* TutorialScene::GetCurrentTutorialMessageSprite() const
{
	if (tutorialDirector_.IsFinished()) {
		return tutorialFinishedMessageSprite_.get();
	}

	int index = tutorialDirector_.GetCurrentIndex();

	if (index < 0 || index >= static_cast<int>(tutorialMessageSprites_.size())) {
		return nullptr;
	}

	return tutorialMessageSprites_[index].get();
}

void TutorialScene::UpdateTutorial(float deltaTime)
{
	TutorialContext context;
	context.player = player_.get();
	context.cameraController = cameraController_.get();
	context.input = Input::GetInstance();
	context.deltaTime = deltaTime;

	tutorialDirector_.Update(context);

	UpdateTutorialScoreBar();
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

void TutorialScene::InitializeTutorialScoreBar()
{
	const float centerX = static_cast<float>(WinApp::kClientWidth) * 0.5f;
	const float y = static_cast<float>(WinApp::kClientHeight) - 72.0f;

	tutorialScoreBarCenterPos_ = { centerX, y };
	tutorialScoreBarSize_ = { 640.0f, 18.0f };

	tutorialScoreBackSprite_ = std::make_unique<Sprite>();
	tutorialScoreBackSprite_->Initialize(
		SpriteCommon::GetInstance(),
		TextureManager::kDefaultTextureName
	);
	tutorialScoreBackSprite_->SetAnchorPoint({ 0.5f, 0.5f });
	tutorialScoreBackSprite_->SetPos(tutorialScoreBarCenterPos_);
	tutorialScoreBackSprite_->SetSize(tutorialScoreBarSize_);
	tutorialScoreBackSprite_->SetColor({ 0.05f, 0.10f, 0.15f, 0.75f });
	tutorialScoreBackSprite_->Update();

	tutorialScoreFillSprite_ = std::make_unique<Sprite>();
	tutorialScoreFillSprite_->Initialize(
		SpriteCommon::GetInstance(),
		TextureManager::kDefaultTextureName
	);
	tutorialScoreFillSprite_->SetAnchorPoint({ 0.0f, 0.5f });
	tutorialScoreFillSprite_->SetPos({
		tutorialScoreBarCenterPos_.x - tutorialScoreBarSize_.x * 0.5f,
		tutorialScoreBarCenterPos_.y
		});
	tutorialScoreFillSprite_->SetSize({ 0.0f, tutorialScoreBarSize_.y });
	tutorialScoreFillSprite_->SetColor({ 0.25f, 0.75f, 1.0f, 0.95f });
	tutorialScoreFillSprite_->Update();
}

void TutorialScene::UpdateTutorialScoreBar()
{
	if (!tutorialScoreBackSprite_ || !tutorialScoreFillSprite_) {
		return;
	}

	float progress = tutorialDirector_.GetCurrentProgress();

	if (progress < 0.0f) {
		progress = 0.0f;
	}
	if (progress > 1.0f) {
		progress = 1.0f;
	}

	tutorialScoreBackSprite_->SetPos(tutorialScoreBarCenterPos_);
	tutorialScoreBackSprite_->SetSize(tutorialScoreBarSize_);
	tutorialScoreBackSprite_->Update();

	tutorialScoreFillSprite_->SetPos({
		tutorialScoreBarCenterPos_.x - tutorialScoreBarSize_.x * 0.5f,
		tutorialScoreBarCenterPos_.y
		});
	tutorialScoreFillSprite_->SetSize({
		tutorialScoreBarSize_.x * progress,
		tutorialScoreBarSize_.y
		});
	tutorialScoreFillSprite_->Update();
}

void TutorialScene::DrawTutorialScoreBar()
{
	if (tutorialScoreBackSprite_) {
		tutorialScoreBackSprite_->Draw();
	}

	if (tutorialScoreFillSprite_) {
		tutorialScoreFillSprite_->Draw();
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

	if (Sprite* messageSprite = GetCurrentTutorialMessageSprite()) {
		messageSprite->Draw();
	}

	if (pauseMenu_) {
		pauseMenu_->Draw();
	}

	DrawTutorialScoreBar();

	DebugRenderer::GetInstance()->RenderAll(*camera_);
}

void TutorialScene::DrawImGui()
{
#ifdef USE_IMGUI
	ImGui::Begin("Tutorial");

	if (tutorialDirector_.IsFinished()) {
		ImGui::TextUnformatted("Tutorial Finished");
		ImGui::Separator();
		ImGui::TextUnformatted("ESCメニューからタイトルに戻れます");
	}
	else {
		ImGui::TextUnformatted(tutorialDirector_.GetCurrentTitle().c_str());
		ImGui::Separator();
		ImGui::TextWrapped("%s", tutorialDirector_.GetCurrentMessage().c_str());
		ImGui::Text("Task Index: %d", tutorialDirector_.GetCurrentIndex());
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