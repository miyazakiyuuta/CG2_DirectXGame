#include "scene/GamePlayScene.h"

#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/DebugCamera.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "base/ImGuiManager.h"
#include "effect/CylinderManager.h"
#include "effect/ParticleManager.h"
#include "effect/RingManager.h"
#include "io/Input.h"

#include "2d/Sprite.h"
#include "3d/Object3d.h"
#include "3d/Skybox.h"
#include "CameraController.h"
#include "Player.h"
#include "Slug.h"
#include "StageEditor.h"
#include "Tongue.h"
#include "debug/DebugGrid.h"
#include "effect/ParticleEmitter.h"

#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({std::numbers::pi_v<float> / 10.0f, 0.0f, 0.0f});
	camera_->SetTranslate({0.0f, 7.5f, -20.0f});

	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());

	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	ParticleManager::GetInstance()->SetCamera(camera_.get());

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");

	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("Cube.obj");

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

	stageEditor_ = std::make_unique<StageEditor>(Object3dCommon::GetInstance(), camera_.get());
	stageEditor_->Initialize("Cube.obj");
	stageEditor_->Load("resources/stage.json");

	Vector3 playerStart = {0.0f, 3.0f, 0.0f};
	if (auto spawn = stageEditor_->GetPlayerSpawnPosition()) {
		playerStart = *spawn;
	}
	player_ = std::make_unique<Player>();
	player_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Cube.obj", playerStart);

	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(camera_.get());
	cameraController_->SetObstacleColliders(&stageBlockColliders_);

	bugs_.clear();
	for (const auto& spawnPos : stageEditor_->GetBugSpawnPositions()) {
		auto bug = std::make_unique<Bug>();
		bug->Initialize(camera_.get());
		bug->SetPositionImmediate(spawnPos);
		bugs_.push_back(std::move(bug));
	}
	// ナメクジの初期化 (DirectXCommonのインスタンスを渡すように変更)
	slug_ = std::make_unique<Slug>();
	slug_->Initialize(
	    DirectXCommon::GetInstance(), // 追加
	    Object3dCommon::GetInstance(), camera_.get(), "sphere.obj");
	slug_->SetPosition({5.0f, 0.5f, 5.0f});
	// ブロックの上面（通常 Y=1.0）より少し上に配置して埋まりを確実に防止
	slug_->SetPosition({5.0f, 1.2f, 5.0f});

	slug_->SetBodyColor({0.8f, 0.2f, 0.2f, 1.0f});  // 目立つように赤系
	slug_->SetTrailColor({1.0f, 0.0f, 1.0f, 1.0f}); // ネオンピンクのヌルヌル

	debugGrid_ = std::make_unique<DebugGrid>();
	debugGrid_->Initialize(DirectXCommon::GetInstance());
}

void GamePlayScene::Finalize() {
	// 終了処理の実体を追加（LNK2001エラー対策）
}

void GamePlayScene::Update() {
#ifdef USE_IMGUI
	imGuiManager_->Begin();
#endif

	stageEditor_->Update();
	stageBlockColliders_ = stageEditor_->GetBlockAABBs();
	waterBlockColliders_ = stageEditor_->GetWaterBlockAABBs();
	player_->SetBlockColliders(&stageBlockColliders_);

	if (!stageEditor_->IsEditMode()) {
		player_->Update(cameraController_->GetYaw());
		cameraController_->Update(player_->GetPosition());

		if (slug_) {
			slug_->Update(1.0f / 60.0f);
		}

		for (auto& bug : bugs_) {
			bug->Update();
		}

		if (player_) {
			Tongue* tongue = player_->GetTongue();
			if (tongue && tongue->CanHitBug()) {
				const CollisionUtility::Sphere tongueSphere = tongue->GetHitSphere();
				for (auto& bug : bugs_) {
					if (CollisionUtility::IntersectSphere_Sphere(tongueSphere, bug->GetHitSphere())) {
						player_->AddChargeStock(1);
						bug->OnTongueHit();
						tongue->Reset();
						break;
					}
				}
			}
		}
	} else {
		cameraController_->Update(player_->GetPosition());
	}

	player_->UpdateTransparencyByCamera(camera_->GetTranslate());

	camera_->Update();
	camera_->TransferToGPU();

#ifdef USE_IMGUI
	imGuiManager_->End();
#endif
}

void GamePlayScene::Draw() {
	skybox_->Draw(*camera_);

	// --- 不透明オブジェクトの描画 ---
	stageEditor_->Draw();
	player_->Draw();
	for (auto& bug : bugs_) {
		bug->Draw();
	}

	if (slug_) {
		slug_->Draw(); // 本体
	}

 // 半透明 (引数にカメラが必要になったため修正)
	if (slug_) {
		slug_->DrawTransparent(*camera_);
	}


	debugGrid_->Draw(*camera_);

#ifdef USE_IMGUI
	imGuiManager_->Draw();
#endif
}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;