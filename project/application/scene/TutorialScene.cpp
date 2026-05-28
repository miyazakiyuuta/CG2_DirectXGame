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
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "base/WinApp.h"
#include "debug/DebugGrid.h"
#include "debug/DebugRenderer.h"
#include "effect/ParticleManager.h"
#include "io/Input.h"
#include "math/Transform.h"
#include "scene/SceneManager.h"
#include "Tongue.h"
#include "Enemy/Types/SentinelEnemy.h"
#include "utility/Logger.h"

#include "Enemy/Manager/EnemyManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

#include <algorithm>
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

	ParticleManager::GetInstance()->Initialize(
		DirectXCommon::GetInstance(),
		SrvManager::GetInstance()
	);
	ParticleManager::GetInstance()->SetCamera(camera_.get());
	ParticleManager::GetInstance()->EnsureParticleGroup("break", "resources/uvChecker.png");
	// XPオーブ用。ここでブレンドモードを指定する
	ParticleManager::GetInstance()->CreateParticleGroup(
		"xp_orb",
		"resources/circle.png",
		BlendMode::Add
	);

	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");
	TextureManager::GetInstance()->LoadTexture("resources/circle.png");

	ModelManager::GetInstance()->LoadModel("Cube.obj");
	ModelManager::GetInstance()->LoadModel("tongue/tongue.obj");
	ModelManager::GetInstance()->LoadModel("Frog", "Frog.gltf");
	ModelManager::GetInstance()->LoadModel("well", "well.obj");

	ModelManager::GetInstance()->LoadModel("Enemy/ClusterMinion", "ClusterMinion.obj");
	ModelManager::GetInstance()->LoadModel("Enemy/GhostFace", "GhostFace.obj");
	ModelManager::GetInstance()->LoadModel("Enemy/ProminenceSensor", "ProminenceSensor.obj");
	ModelManager::GetInstance()->LoadModel("Enemy/SentinelHook", "SentinelHook.obj");

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

	ParticleManager::GetInstance()->CreateParticleGroup("xp_orb", "resources/circle.png", BlendMode::Add);

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

	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(Object3dCommon::GetInstance(), camera_.get());
	enemyManager_->SetBlockColliders(&stageBlockColliders_);
	enemyManager_->SetKeepInsideCylinder(&wellCylinder_);
	enemyManager_->SetOnEnemyDeadCallback([this](BaseEnemy* deadEnemy) {
		// チュートリアル用センチネルは「敵を倒す」カウントに入れない。
		// センチネルフックタスク中なら、少し待って再生成する。
		if (deadEnemy == tutorialSentinelEnemy_) {
			tutorialSentinelEnemy_ = nullptr;

			if (IsSentinelHookTutorialActive()) {
				tutorialSentinelRespawnRequested_ = true;
				tutorialSentinelRespawnTimer_ = tutorialSentinelRespawnDelay_;
			}
			return;
		}

		++tutorialEnemyKilledCount_;
		});
	enemyManager_->SetXPOrbSpawner(
		[this](const Vector3& pos, AbilityId ability, int amount) {
			XPOrb orb;
			orb.Init(pos, amount);
			orb.SetAbility(ability);
			orb.SetGroundY(stage_ ? stage_->GetHeightAt(pos) : 0.0f);
			xpOrbs_.push_back(orb);
		}
	);

	player_->SetEnemyManager(enemyManager_.get());

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

	UpdateStageColliders();

	BuildTutorialStepDefinitions();
	GenerateTutorialMessageSprites();
	SetupTutorialTasksFromDefinitions();
	tutorialDirector_.Start();

	InitializeTutorialScoreBar();
	UpdateTutorialScoreBar();

	// --- BGMの読み込みとループ再生 ---
	tutorialBgm_ = SoundManager::GetInstance()->LoadFile(tutorialBgmPath_);

	tutorialBgmHandle_ = SoundManager::GetInstance()->PlayWave(
		tutorialBgm_,
		true,
		SoundManager::SoundCategory::BGM
	);

	// 必要なら音量をここで調整
	SoundManager::GetInstance()->SetCategoryVolume(
		SoundManager::SoundCategory::BGM,
		0.5f
	);

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
	ClearTutorialPhaseRuntime(true);

	ParticleManager::GetInstance()->SetCamera(nullptr);

	if (tutorialBgmHandle_ != SoundManager::InvalidHandle) {
		SoundManager::GetInstance()->StopWave(tutorialBgmHandle_);
		tutorialBgmHandle_ = SoundManager::InvalidHandle;
	}

	SoundManager::GetInstance()->Unload(tutorialBgmPath_);

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
		ToUtf8String(u8"WASDで移動してみよう"),
		ToUtf8String(u8"左スティックで移動してみよう"),
		60,
		[](const TutorialContext& ctx) {
			if (!ctx.player) {
				return 0;
			}

			Vector3 v = ctx.player->GetVelocity();
			const float speedSq = v.x * v.x + v.z * v.z;
			const float threshold = 0.01f;

			return speedSq > threshold ? 1 : 0;
		},
		nullptr,
		nullptr,
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"ジャンプ",
		ToUtf8String(u8"Space長押しでジャンプを溜めよう"),
		ToUtf8String(u8"Aボタン長押しでジャンプを溜めよう"),
		90,
		[](const TutorialContext& ctx) {
			if (!ctx.input) {
				return 0;
			}

			const bool keyboard = ctx.input->IsPushKey(DIK_SPACE);
			const bool gamepad = ctx.input->IsPressPad(XINPUT_GAMEPAD_A);

			return (keyboard || gamepad) ? 1 : 0;
		},
		nullptr,
		nullptr,
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"エイム",
		ToUtf8String(u8"右クリックで1秒間一人称にしてみよう"),
		ToUtf8String(u8"左トリガーで1秒間一人称にしてみよう"),
		60,
		[](const TutorialContext& ctx) {
			if (!ctx.cameraController) {
				return 0;
			}

			return ctx.cameraController->IsAimMode() ? 1 : 0;
		},
		nullptr,
		nullptr,
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"舌",
		ToUtf8String(u8"左クリックで舌を3回伸ばしてみよう"),
		ToUtf8String(u8"右トリガーで舌を3回伸ばしてみよう"),
		3,
		[](const TutorialContext& ctx) {
			return ctx.tongueShotStartedThisFrame ? 1 : 0;
		},
		nullptr,
		nullptr,
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"側面張り付き",
		ToUtf8String(u8"舌を伸ばして壁の側面に張り付いてみよう"),
		ToUtf8String(u8"舌を伸ばして壁の側面に張り付いてみよう"),
		30,
		[](const TutorialContext& ctx) {
			if (!ctx.player) {
				return 0;
			}

			return ctx.player->IsWallClinging() ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		nullptr,
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"側面登り",
		ToUtf8String(u8"張り付いたままWで上へ登ってみよう"),
		ToUtf8String(u8"張り付いたまま\n左スティックを上に倒して登ってみよう"),
		60,
		[](const TutorialContext& ctx) {
			if (!ctx.player || !ctx.input) {
				return 0;
			}

			if (!ctx.player->IsWallClinging()) {
				return 0;
			}

			const bool keyboardUp = ctx.input->IsPushKey(DIK_W);
			const float stickY = ctx.input->GetLeftStickY();
			const bool gamepadUp = stickY > 0.5f;

			return (keyboardUp || gamepadUp) ? 1 : 0;
		},
		nullptr,
		nullptr,
		0.5f
		});

	tutorialStepDefinitions_.push_back({
	"スタミナ",
	ToUtf8String(u8"張り付き中はスタミナを消費します"),
	ToUtf8String(u8"張り付き中はスタミナを消費します"),
	50,
	[this](const TutorialContext& ctx) {
		if (!ctx.player) {
			return 0;
		}

		const float currentGauge = ctx.player->GetWallClingGauge();
		const float consumedThisFrame =
			tutorialWallClingPrevGauge_ - currentGauge;

		tutorialWallClingPrevGauge_ = currentGauge;

		if (!ctx.player->IsClinging()) {
			return 0;
		}

		if (consumedThisFrame <= 0.0f) {
			return 0;
		}

		float scaledScore =
			consumedThisFrame * tutorialWallClingScoreScale_ +
			tutorialWallClingConsumeRemainder_;

		int addScore = static_cast<int>(scaledScore);
		tutorialWallClingConsumeRemainder_ =
			scaledScore - static_cast<float>(addScore);

		return addScore;
	},
	[this]() {
		tutorialWallClingConsumeRemainder_ = 0.0f;

		if (player_) {
			tutorialWallClingPrevGauge_ = player_->GetWallClingGauge();
		}
		else {
			tutorialWallClingPrevGauge_ = 0.0f;
		}
	},
	nullptr,
	0.8f
		});

	tutorialStepDefinitions_.push_back({
		"下面張り付き",
		ToUtf8String(u8"天井の下面はSpaceで走ろう"),
		ToUtf8String(u8"天井の下面はAボタンで走ろう"),
		30,
		[](const TutorialContext& ctx) {
			if (!ctx.player) {
				return 0;
			}

			return ctx.player->IsCeilingCrawling() ? 1 : 0;
		},
		nullptr,
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
	"下面から側面へ",
	ToUtf8String(u8"下面を走って側面へ移動してみよう"),
	ToUtf8String(u8"下面を走って側面へ移動してみよう"),
	1,
	[this](const TutorialContext& ctx) {
		if (!ctx.player || !ctx.input) {
			return 0;
		}

		const bool keyboardRun =
			ctx.input->IsPushKey(DIK_SPACE);

		const bool gamepadRun =
			ctx.input->IsPressPad(XINPUT_GAMEPAD_A);

		const bool runningInput =
			keyboardRun || gamepadRun;

		// 下面にいる間に走る入力をしたことを覚える
		if (ctx.player->IsCeilingCrawling() && runningInput) {
			tutorialCeilingToWallSawCeilingRun_ = true;
		}

		// その後、側面張り付きへ移ったら達成
		if (tutorialCeilingToWallSawCeilingRun_ &&
			ctx.player->IsWallClinging()) {
			return 1;
		}

		return 0;
	},
	[this]() {
		tutorialCeilingToWallSawCeilingRun_ = false;
	},
	[this]() {
		tutorialCeilingToWallSawCeilingRun_ = false;
		ClearTutorialPhaseRuntime(true);
	},
	0.5f
		});

	tutorialStepDefinitions_.push_back({
		"ソナー",
		ToUtf8String(u8"Fキーでソナーを使ってみよう"),
		ToUtf8String(u8"Yボタンでソナーを使ってみよう"),
		30,
		[](const TutorialContext& ctx) {
			if (!ctx.player) {
				return 0;
			}

			return ctx.player->IsSonarActive() ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
			SpawnTutorialWeakEnemy();
			SpawnTutorialClingBlocks();
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		2.5f
		});

	tutorialStepDefinitions_.push_back({
		"擬態",
		ToUtf8String(u8"     Qキーの後に舌でブロックに張り付いて\n       擬態を使ってみよう"),
		ToUtf8String(u8"     Xボタンの後舌でにブロックに張り付いて\n       擬態を使ってみよう"),
		1,
		[](const TutorialContext& ctx) {
			if (!ctx.input || !ctx.player) {
				return 0;
			}

			return (ctx.player->IsMimicking()) ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"敵を倒す",
		ToUtf8String(u8"Eキーで敵を倒してみよう"),
		ToUtf8String(u8"Bボタンで敵を倒してみよう"),
		1,
		[this](const TutorialContext&) {
			return tutorialEnemyKilledCount_ > 0 ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
			SpawnTutorialWeakEnemy();
		},
		[this]() {
			ClearTutorialEnemies();
			ClearTutorialPhaseObjects();
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"XP",
		ToUtf8String(u8"敵が落としたXPを拾ってみよう"),
		ToUtf8String(u8"敵が落としたXPを拾ってみよう"),
		1,
		[this](const TutorialContext&) {
			return tutorialXPCollectedCount_ > 0 ? 1 : 0;
		},
[this]() {
	tutorialXPCollectedCount_ = 0;

	// 直前の敵のXPをすでに拾っていても詰まないようにする。
	// xpOrbs_ が空でなくても、拾われたXPは inactive のまま残るため、
	// vector の空判定ではなく active なXPがあるかを見る。
	if (!HasActiveTutorialXPOrb()) {
		SpawnTutorialXPOrbForTask();
	}
},
		[this]() {
			ClearTutorialXP();
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"壊れるブロック",
		ToUtf8String(u8"舌攻撃で壊れるブロックを壊そう"),
		ToUtf8String(u8"Bボタン攻撃で壊れるブロックを壊そう"),
		1,
		[this](const TutorialContext&) {
			return CountAliveTutorialObjects(BlockID::Breakable) == 0 ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
			SpawnTutorialBreakableBlocks();
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"ワープブロック",
		ToUtf8String(u8"ワープブロックに入ってみよう"),
		ToUtf8String(u8"ワープブロックに入ってみよう"),
		1,
		[this](const TutorialContext&) {
			return tutorialWarpUsedCount_ > 0 ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
			SpawnTutorialWarpBlock();
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		0.5f
		});

	tutorialStepDefinitions_.push_back({
		"センチネルフック",
		ToUtf8String(u8"  センチネル(緑の敵)に\n   舌を当ててフック移動しよう"),
		ToUtf8String(u8"  センチネル(緑の敵)に\n   舌を当ててフック移動しよう"),
		60,
		[](const TutorialContext& ctx) {
			if (!ctx.player) {
				return 0;
			}

			return ctx.player->IsTonguePulling() ? 1 : 0;
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
			SpawnTutorialSentinelHook();
		},
		[this]() {
			ClearTutorialPhaseRuntime(true);
		},
		3.0f
		});
}

void TutorialScene::SetupTutorialTasksFromDefinitions()
{
	tutorialDirector_.Clear();

	for (const auto& def : tutorialStepDefinitions_) {
		TutorialTask task;
		task.title = def.title;
		task.message = def.keyboardMessage;
		task.requiredScore = def.requiredScore;
		task.scoreDelta = def.scoreDelta;

		task.onEnter = [onEnter = def.onEnter](const TutorialContext&) {
			if (onEnter) {
				onEnter();
			}
			};

		task.onExit = [onExit = def.onExit](const TutorialContext&) {
			if (onExit) {
				onExit();
			}
			};

		task.completeWaitSeconds = def.completeWaitSeconds;

		tutorialDirector_.AddTask(task);
	}
}

void TutorialScene::GenerateTutorialMessageSprites()
{
	tutorialKeyboardMessageSprites_.clear();
	tutorialGamepadMessageSprites_.clear();

	tutorialKeyboardMessageSprites_.reserve(tutorialStepDefinitions_.size());
	tutorialGamepadMessageSprites_.reserve(tutorialStepDefinitions_.size());

	const std::string prefix = "resources/generated_ui/tutorial_";

	for (size_t i = 0; i < tutorialStepDefinitions_.size(); ++i) {
		const std::string keyboardPath =
			prefix + "keyboard_message_" + std::to_string(i) + ".png";

		const std::string gamepadPath =
			prefix + "gamepad_message_" + std::to_string(i) + ".png";

		tutorialKeyboardMessageSprites_.push_back(
			CreateTutorialMessageSprite(
				tutorialStepDefinitions_[i].keyboardMessage,
				keyboardPath
			)
		);

		tutorialGamepadMessageSprites_.push_back(
			CreateTutorialMessageSprite(
				tutorialStepDefinitions_[i].gamepadMessage,
				gamepadPath
			)
		);
	}

	tutorialKeyboardFinishedMessageSprite_ = CreateTutorialMessageSprite(
		ToUtf8String(u8"チュートリアル完了！\nESCメニューからタイトルに戻れます"),
		prefix + "keyboard_message_finished.png"
	);

	tutorialGamepadFinishedMessageSprite_ = CreateTutorialMessageSprite(
		ToUtf8String(u8"チュートリアル完了！\nSTARTメニューからタイトルに戻れます"),
		prefix + "gamepad_message_finished.png"
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
	const bool useGamepad =
		lastInputDevice_ == TutorialInputDevice::Gamepad;

	if (tutorialDirector_.IsFinished()) {
		return useGamepad
			? tutorialGamepadFinishedMessageSprite_.get()
			: tutorialKeyboardFinishedMessageSprite_.get();
	}

	const int index = tutorialDirector_.GetCurrentIndex();

	if (index < 0 || index >= static_cast<int>(tutorialStepDefinitions_.size())) {
		return nullptr;
	}

	if (useGamepad) {
		if (index >= static_cast<int>(tutorialGamepadMessageSprites_.size())) {
			return nullptr;
		}
		return tutorialGamepadMessageSprites_[index].get();
	}

	if (index >= static_cast<int>(tutorialKeyboardMessageSprites_.size())) {
		return nullptr;
	}
	return tutorialKeyboardMessageSprites_[index].get();
}

StageObject TutorialScene::MakeTutorialBlock(
	BlockID blockId,
	const Vector3& position,
	const Vector3& scale,
	const Vector4& color,
	int hp
) const {
	StageObject object;
	object.id = -1;
	object.modelName = "Cube.obj";
	object.blockId = blockId;
	object.position = position;
	object.rotation = { 0.0f, 0.0f, 0.0f };
	object.scale = scale;
	object.color = color;
	object.hp = hp;
	object.warpTargetPosition = { 0.0f, 4.0f, 0.0f };
	object.warpTargetSceneId = -1;
	return object;
}

int TutorialScene::AddTutorialStageObject(const StageObject& source)
{
	if (!stage_) {
		return -1;
	}

	StageObject object = source;
	object.id = tutorialNextObjectId_++;

	stage_->GetStageData().objects.push_back(object);
	stage_->UpdateOrCreateInstance(object);

	tutorialPhaseObjectIds_.push_back(object.id);

	UpdateStageColliders();

	return object.id;
}

void TutorialScene::ClearTutorialPhaseObjects()
{
	if (!stage_) {
		tutorialPhaseObjectIds_.clear();
		return;
	}

	auto& objects = stage_->GetStageData().objects;

	for (int id : tutorialPhaseObjectIds_) {
		stage_->RemoveInstanceById(id);

		objects.erase(
			std::remove_if(
				objects.begin(),
				objects.end(),
				[id](const StageObject& object) {
					return object.id == id;
				}
			),
			objects.end()
		);
	}

	tutorialPhaseObjectIds_.clear();

	UpdateStageColliders();
}

void TutorialScene::ClearTutorialEnemies()
{
	if (enemyManager_) {
		enemyManager_->Clear();
	}

	tutorialSentinelEnemy_ = nullptr;
	tutorialSentinelRespawnRequested_ = false;
	tutorialSentinelRespawnTimer_ = 0.0f;
}

void TutorialScene::ClearTutorialXP()
{
	xpOrbs_.clear();
	tutorialXPCollectedCount_ = 0;
	ParticleManager::GetInstance()->SetExternalInstanceCount("xp_orb", 0);
}

bool TutorialScene::HasActiveTutorialXPOrb() const
{
	for (const auto& orb : xpOrbs_) {
		if (orb.IsActive()) {
			return true;
		}
	}

	return false;
}

void TutorialScene::SpawnTutorialXPOrbForTask()
{
	if (!player_) {
		return;
	}

	Vector3 pos = player_->GetPosition();

	// プレイヤーの真上すぎると見えづらいので、少し前方にずらして上から落とす
	pos.y += 8.0f;
	pos.z += 3.0f;

	XPOrb orb;
	orb.Init(pos, 1);
	orb.SetAbility(AbilityId::JumpPower);
	orb.SetFiniteLife(false);
	orb.SetGroundY(stage_ ? stage_->GetHeightAt(pos) : 0.0f);

	xpOrbs_.push_back(orb);
}

void TutorialScene::ClearTutorialPhaseRuntime(bool clearXP)
{
	ClearTutorialPhaseObjects();
	ClearTutorialEnemies();

	tutorialEnemyKilledCount_ = 0;
	tutorialWarpUsedCount_ = 0;
	warpCooldownCounter_ = 0;
	lastWarpId_ = -1;

	if (clearXP) {
		ClearTutorialXP();
	}
}

void TutorialScene::SpawnTutorialClingBlocks()
{
	ClearTutorialPhaseObjects();

	AddTutorialStageObject(MakeTutorialBlock(
		BlockID::Normal,
		{ 0.0f, 4.0f, 12.0f },
		{ 4.0f, 4.0f, 1.0f },
		{ 0.25f, 0.85f, 0.65f, 1.0f }
	));

	AddTutorialStageObject(MakeTutorialBlock(
		BlockID::Normal,
		{ 0.0f, 8.0f, 18.0f },
		{ 5.0f, 0.8f, 5.0f },
		{ 0.20f, 0.55f, 0.95f, 1.0f }
	));
}

void TutorialScene::SpawnTutorialBreakableBlocks()
{
	ClearTutorialPhaseObjects();


	AddTutorialStageObject(MakeTutorialBlock(
		BlockID::Breakable,
		{ -10.0f, 2.5f, 10.0f },
		{ 1.5f, 1.5f, 1.5f },
		{ 0.95f, 0.45f, 0.25f, 1.0f },
		2
	));

	AddTutorialStageObject(MakeTutorialBlock(
		BlockID::Breakable,
		{ 10.0f, 3.0f, 10.0f },
		{ 2.0f, 2.0f, 2.0f },
		{ 0.95f, 0.35f, 0.20f, 1.0f },
		3
	));
}

void TutorialScene::SpawnTutorialWarpBlock()
{
	ClearTutorialPhaseObjects();

	StageObject warp = MakeTutorialBlock(
		BlockID::Warp,
		{ 0.0f, 10.2f, 14.0f },
		{ 1.5f, 1.2f, 1.5f },
		{ 0.35f, 0.45f, 1.0f, 0.85f }
	);

	warp.warpTargetPosition = { -6.0f, 4.0f, -8.0f };
	warp.warpTargetSceneId = -1;

	AddTutorialStageObject(warp);
}

void TutorialScene::SpawnTutorialWeakEnemy()
{
	if (!enemyManager_) {
		return;
	}

	enemyManager_->Clear();
	tutorialEnemyKilledCount_ = 0;

	enemyManager_->CreateEnemy(EnemyType::Chasing, { 0.0f, 5.0f, 16.0f });
}

void TutorialScene::SpawnTutorialSentinelHook()
{
	if (!enemyManager_) {
		return;
	}

	enemyManager_->Clear();

	tutorialSentinelEnemy_ =
		enemyManager_->CreateEnemy(EnemyType::Sentinel, { 0.0f, 5.0f, 16.0f });

	tutorialSentinelRespawnRequested_ = false;
	tutorialSentinelRespawnTimer_ = 0.0f;

	if (auto* sentinel = dynamic_cast<SentinelEnemy*>(tutorialSentinelEnemy_)) {
		sentinel->SetTutorialHookMode(true);
	}
}

bool TutorialScene::IsSentinelHookTutorialActive() const
{
	if (tutorialDirector_.IsFinished()) {
		return false;
	}

	return tutorialDirector_.GetCurrentTitle() == "センチネルフック";
}

void TutorialScene::UpdateTutorialSentinelRespawn(float deltaTime)
{
	if (!tutorialSentinelRespawnRequested_) {
		return;
	}

	if (!IsSentinelHookTutorialActive()) {
		tutorialSentinelRespawnRequested_ = false;
		tutorialSentinelRespawnTimer_ = 0.0f;
		return;
	}

	tutorialSentinelRespawnTimer_ -= deltaTime;
	if (tutorialSentinelRespawnTimer_ > 0.0f) {
		return;
	}

	SpawnTutorialSentinelHook();
}

bool TutorialScene::IsTutorialStageObjectAlive(int id) const
{
	if (!stage_) {
		return false;
	}

	for (const auto& object : stage_->GetStageData().objects) {
		if (object.id == id) {
			return true;
		}
	}

	return false;
}

int TutorialScene::CountAliveTutorialObjects(BlockID blockId) const
{
	if (!stage_) {
		return 0;
	}

	int count = 0;

	for (int id : tutorialPhaseObjectIds_) {
		for (const auto& object : stage_->GetStageData().objects) {
			if (object.id == id && object.blockId == blockId) {
				++count;
				break;
			}
		}
	}

	return count;
}

void TutorialScene::UpdateTutorialFrameEvents()
{
	tutorialTongueShotStartedThisFrame_ = false;

	if (!player_ || !player_->GetTongue()) {
		tutorialPrevTongueWasIdle_ = true;
		return;
	}

	const Tongue::State currentTongueState = player_->GetTongue()->GetState();

	tutorialTongueShotStartedThisFrame_ =
		tutorialPrevTongueWasIdle_ &&
		(currentTongueState == Tongue::State::Extending);

	tutorialPrevTongueWasIdle_ =
		(currentTongueState == Tongue::State::Idle);
}

void TutorialScene::UpdateTutorial(float deltaTime)
{
	TutorialContext context;
	context.player = player_.get();
	context.cameraController = cameraController_.get();
	context.input = Input::GetInstance();
	context.deltaTime = deltaTime;
	context.tongueShotStartedThisFrame = tutorialTongueShotStartedThisFrame_;

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

	if (enemyManager_) {
		enemyManager_->SetBlockColliders(&stageBlockColliders_);
		enemyManager_->SetKeepInsideCylinder(&wellCylinder_);
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
	if (tutorialDirector_.IsFinished()) {
		return;
	}

	if (tutorialScoreBackSprite_) {
		tutorialScoreBackSprite_->Draw();
	}

	if (tutorialScoreFillSprite_) {
		tutorialScoreFillSprite_->Draw();
	}
}

bool TutorialScene::HasKeyboardMouseTutorialInput() const
{
	Input* input = Input::GetInstance();
	if (!input) {
		return false;
	}

	return
		input->IsPushKey(DIK_W) ||
		input->IsPushKey(DIK_A) ||
		input->IsPushKey(DIK_S) ||
		input->IsPushKey(DIK_D) ||
		input->IsPushKey(DIK_SPACE) ||
		input->IsTriggerKey(DIK_E) ||
		input->IsTriggerKey(DIK_Q) ||
		input->IsTriggerKey(DIK_F) ||
		input->IsTriggerMouse(0) ||
		input->IsTriggerMouse(1);
}

bool TutorialScene::HasGamepadTutorialInput() const
{
	Input* input = Input::GetInstance();
	if (!input) {
		return false;
	}

	const float stickX = input->GetLeftStickX();
	const float stickY = input->GetLeftStickY();

	const bool stickMoved =
		(stickX * stickX + stickY * stickY) > 0.15f * 0.15f;

	return
		stickMoved ||
		input->IsPressPad(XINPUT_GAMEPAD_A) ||
		input->IsPressPad(XINPUT_GAMEPAD_B) ||
		input->IsPressPad(XINPUT_GAMEPAD_X) ||
		input->IsPressPad(XINPUT_GAMEPAD_Y) ||
		input->GetRightTrigger() >= 0.5f;
}

void TutorialScene::UpdateLastInputDevice()
{
	if (HasGamepadTutorialInput()) {
		lastInputDevice_ = TutorialInputDevice::Gamepad;
		return;
	}

	if (HasKeyboardMouseTutorialInput()) {
		lastInputDevice_ = TutorialInputDevice::KeyboardMouse;
	}
}

void TutorialScene::UpdateTutorialEnemies(float deltaTime)
{
	if (!enemyManager_) {
		return;
	}

	enemyManager_->SetBlockColliders(&stageBlockColliders_);
	enemyManager_->SetKeepInsideCylinder(&wellCylinder_);
	enemyManager_->Update(deltaTime, player_.get());
}

void TutorialScene::UpdateTutorialXPOrbs(float deltaTime)
{
	if (!player_) {
		return;
	}

	for (auto& orb : xpOrbs_) {
		if (!orb.IsActive()) {
			continue;
		}

		if (stage_) {
			orb.SetGroundY(stage_->GetHeightAt(orb.GetPosition()));
		}

		int collected = orb.Update(deltaTime, player_->GetPosition());
		if (collected > 0) {
			tutorialXPCollectedCount_ += collected;
			player_->EnqueueAbilityXP(orb.GetAbility(), static_cast<float>(collected));
		}
	}
}

void TutorialScene::WriteTutorialXPOrbInstances()
{

	uint32_t maxInst = 0;
	auto* instPtr = ParticleManager::GetInstance()->GetInstancingDataWritePtr("xp_orb", maxInst);

	if (!instPtr || maxInst == 0 || !camera_) {
		ParticleManager::GetInstance()->SetExternalInstanceCount("xp_orb", 0);
		return;
	}

	uint32_t numInst = 0;
	const Matrix4x4 viewProj = camera_->GetViewMatrix() * camera_->GetProjectionMatrix();
	Matrix4x4 cameraMatrix = camera_->GetWorldMatrix();

	for (const auto& orb : xpOrbs_) {
		if (!orb.IsActive()) {
			continue;
		}

		if (numInst >= maxInst) {
			break;
		}

		orb.FillInstanceData(instPtr[numInst], cameraMatrix, viewProj);
		++numInst;
	}

	ParticleManager::GetInstance()->SetExternalInstanceCount("xp_orb", numInst);
}

void TutorialScene::UpdateTutorialWarp()
{
	if (!player_ || !stage_) {
		return;
	}

	const CollisionUtility::OBB playerObb =
		player_->GetPlayerOBB(player_->GetPosition());

	for (const auto& object : stage_->GetStageData().objects) {
		// ワープブロック以外は無視
		if (object.blockId != BlockID::Warp) {
			continue;
		}

		Transform t;
		t.translate = object.position;
		t.rotate = object.rotation;
		t.scale = object.scale;

		// 判定を少し大きめにして、プレイヤーがワープに入りやすくする
		const float warpInflation = 1.5f;
		CollisionUtility::OBB warpObb =
			CollisionUtility::MakeOBBFromTransform(
				t,
				{
					object.scale.x * warpInflation,
					object.scale.y * warpInflation,
					object.scale.z * warpInflation
				}
			);

		if (!CollisionUtility::IntersectOBB_OBB(playerObb, warpObb)) {
			continue;
		}

		// クールダウンがゼロ、または直前に入ったワープとは違う場合
		if (warpCooldownCounter_ == 0 || lastWarpId_ != object.id) {

			// --- Step 1: 出口アクターの動的生成 ---
			auto warpExit = std::make_unique<WarpExit>();
			warpExit->Initialize(
				Object3dCommon::GetInstance(),
				camera_.get(),
				object.warpTargetPosition,
				"Cube.obj"
			);
			activeWarpExits_.push_back(std::move(warpExit));

			// --- Step 2: プレイヤーのワープ移動を開始 ---
			player_->SetPendingTeleport(object.warpTargetPosition);
			lastWarpId_ = object.id;
			warpCooldownCounter_ = 90;
			++tutorialWarpUsedCount_;

			Logger::Log(
				std::string("Tutorial Warped to ") + std::to_string(object.warpTargetPosition.x) + "," + std::to_string(object.warpTargetPosition.y) + "," + std::to_string(object.warpTargetPosition.z) + "\n");
		}
		break;
	}

	if (warpCooldownCounter_ > 0) {
		--warpCooldownCounter_;
	}
	else {
		lastWarpId_ = -1;
	}

	// --- 生成された出口アクターの更新と破棄処理 ---
	for (auto& exit : activeWarpExits_) {
		exit->Update(player_->GetPosition());
	}
	
	activeWarpExits_.erase(
		std::remove_if(activeWarpExits_.begin(), activeWarpExits_.end(),
			[](const std::unique_ptr<WarpExit>& e) { return e->IsExpired(); }),
		activeWarpExits_.end()
	);
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

		if (pauseMenu_->IsRestartRequested()) {
			SceneManager::GetInstance()->ChangeScene("TUTORIAL");
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

		UpdateTutorialFrameEvents();
		UpdateTutorialWarp();
		UpdateTutorialEnemies(deltaTime);
		UpdateTutorialXPOrbs(deltaTime);

		UpdateLastInputDevice();
		UpdateTutorial(deltaTime);
		UpdateTutorialSentinelRespawn(deltaTime);
	}

	if (camera_) {
		camera_->Update();
		camera_->TransferToGPU();
	}

	if (wellObject_) {
		wellObject_->Update();
	}

	ParticleManager::GetInstance()->Update(deltaTime);
	WriteTutorialXPOrbInstances();

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

	if (player_) {
		player_->Draw();
	}

	if (enemyManager_) {
		enemyManager_->Draw();
	}

	if (stage_) {
		stage_->Draw();
	}

	for (const auto& exit : activeWarpExits_) {
		exit->Draw();
	}

	ParticleManager::GetInstance()->Draw();

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

	DrawTutorialScoreBar();

	if (pauseMenu_) {
		pauseMenu_->Draw();
	}

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
		ImGui::Text("Score: %d / %d",
			tutorialDirector_.GetCurrentScore(),
			tutorialDirector_.GetCurrentRequiredScore()
		);
	}

	ImGui::Separator();
	ImGui::Text("EnemyKilled: %d", tutorialEnemyKilledCount_);
	ImGui::Text("XPCollected: %d", tutorialXPCollectedCount_);
	ImGui::Text("WarpUsed: %d", tutorialWarpUsedCount_);
	ImGui::Text("PhaseObjects: %d", static_cast<int>(tutorialPhaseObjectIds_.size()));

	ImGui::End();

	if (player_) {
		player_->DrawImGui();
	}

	if (cameraController_) {
		cameraController_->DrawImGui();
	}

	if (enemyManager_) {
		enemyManager_->DrawImGui();
	}
#endif
}