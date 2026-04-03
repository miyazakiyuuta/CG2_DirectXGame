#include "scene/GamePlayScene.h"

#include "io/Input.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "2d/TextureManager.h"
#include "3d/DebugCamera.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "effect/CylinderManager.h"
#include "effect/ParticleConfig.h"
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
#include "StageSerializer.h"
#include "Tongue.h"
#include "debug/DebugGrid.h"
#include "effect/ParticleEmitter.h"
#include "math/Transform.h"
// エネミーのインクルードを追加
#include "Enemy/Manager/EnemyManager.h"
#include "debug/DebugGrid.h"
#include "debug/DebugRenderer.h"
#include "effect/ParticleEmitter.h"

#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "utility/Logger.h"
#include <sstream>
#include <numbers>

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f, 0.0f, 0.0f });
	camera_->SetTranslate({ 0.0f, 7.5f, -20.0f });

	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	ParticleManager::GetInstance()->SetCamera(camera_.get());
	// simple particle group used for block destruction
	ParticleManager::GetInstance()->CreateParticleGroup("break", "resources/uvChecker.png");

	debugCamera_ = std::make_unique<DebugCamera>();
	debugCamera_->Initialize();

	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/grass.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("human", "sneakWalk.gltf");
	ModelManager::GetInstance()->LoadModel("Kanban1.obj");
	ModelManager::GetInstance()->LoadModel("Cube.obj");
	ModelManager::GetInstance()->LoadModel("human", "human_re.gltf");
	ModelManager::GetInstance()->LoadModel("Frog", "Frog.gltf");

	// Load the single well model so it can be placed in the scene
	ModelManager::GetInstance()->LoadModel("well", "well.obj");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("human_re.gltf");
	// object3d_->SetModel("Frog.gltf");
	object3d_->SetCamera(camera_.get());
	object3d_->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d_->SetColor({ 0.5f, 0.5f, 0.5f, 1.0f });
	object3d_->SetUseEnvironmentMap(true); // 環境マップ

	// Create the well object and place it at a fixed position only if model is loaded
	if (Object3dCommon::GetInstance() && camera_) {
		// Ensure model was actually loaded
		if (ModelManager::GetInstance()->FindModel("well.obj")) {
			wellObject_ = std::make_unique<Object3d>();
			wellObject_->Initialize(Object3dCommon::GetInstance());
			wellObject_->SetModel("well.obj");
			wellObject_->SetCamera(camera_.get());
			// Adjust this position as needed
			// Move the well slightly further from the camera and make it very small
			wellObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
			wellObject_->SetScale({ 60.0f, 60.0f, 60.0f });
			wellObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
			if (wellObject_) {
				Vector3 wellPos = wellObject_->GetTranslate();
				Vector3 wellScale = wellObject_->GetScale();

				wellCylinder_.center = wellPos;
				wellCylinder_.radius = 58.5f;
				wellCylinder_.halfHeight = 1000.0f;
			}
		} else {
			// Model not found; skip creating wellObject_
			wellObject_.reset();
		}
	} else {
		wellObject_.reset();
	}

	std::string envMapPath = "resources/rostock_laage_airport_4k.dds";
	TextureManager::GetInstance()->LoadTexture(envMapPath);
	uint32_t envSrvIndex = TextureManager::GetInstance()->GetSrvIndex(envMapPath);
	Object3dCommon::GetInstance()->SetEnvironmentSrvIndex(envSrvIndex);

	skybox_ = std::make_unique<Skybox>();
	skybox_->Initialize(DirectXCommon::GetInstance(), envMapPath);

	// 実行時ステージを生成
	stage_ = std::make_unique<Stage>(Object3dCommon::GetInstance(), camera_.get());

	// ステージファイルの読み込みは GamePlayScene が担当
	auto loadedStage = StageSerializer::LoadFromFile("resources/stage.json");
	if (loadedStage) {
		stage_->SetStageData(*loadedStage);
	}

	// プレイヤー開始位置を Stage から取得
	Vector3 playerStart = { 0.0f, 3.0f, 0.0f };
	if (auto spawn = stage_->GetPlayerSpawnPosition()) {
		playerStart = *spawn;
	}

	player_ = std::make_unique<Player>();
	player_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Cube.obj", playerStart);
	// give player a reference to the stage for abilities (camouflage lookup / sonar)
	player_->SetStage(stage_.get());

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
	player_->SetMovementLimitCylinder(&wellCylinder_);

	reticle_ = std::make_unique<Reticle>();
	reticle_->Initialize(
		SpriteCommon::GetInstance(),
		camera_.get(),
		cameraController_.get(),
		player_.get(),
		&stageBlockColliders_
	);

	// StageEditor は Stage を受け取って編集するだけ
	stageEditor_ = std::make_unique<StageEditor>(stage_.get(), Object3dCommon::GetInstance(), camera_.get());
	stageEditor_->Initialize("Cube.obj");

	// 虫の生成と初期化
	bugs_.clear();
	for (const auto& spawnPos : stage_->GetBugSpawnPositions()) {
		auto bug = std::make_unique<Bug>();
		bug->Initialize(camera_.get());
		bug->SetPositionImmediate(spawnPos);
		bugs_.push_back(std::move(bug));
	}

	// ナメクジの初期化
	slug_ = std::make_unique<Slug>();
	slug_->Initialize(DirectXCommon::GetInstance(), Object3dCommon::GetInstance(), camera_.get(), "sphere.obj");
	slug_->SetPosition({ 5.0f, 0.5f, 5.0f });
	slug_->SetPosition({ 5.0f, 1.2f, 5.0f });

	slug_->SetBodyColor({ 0.8f, 0.2f, 0.2f, 1.0f });
	slug_->SetTrailColor({ 1.0f, 0.0f, 1.0f, 1.0f });

	// --- エネミーマネージャーの初期化 ---
	enemyManager_ = std::make_unique<EnemyManager>();
	enemyManager_->Initialize(Object3dCommon::GetInstance(), camera_.get());

	// 【エネミー出現位置】Y座標を5.0にしているので、空から降ってきて地面に着地します。
	enemyManager_->CreateEnemy(EnemyType::Chasing, { 10.0f, 5.0f, 10.0f });     // 追尾（赤）
	enemyManager_->CreateEnemy(EnemyType::Shooting, { -10.0f, 5.0f, 15.0f });   // 射撃（青）
	enemyManager_->CreateEnemy(EnemyType::Sentinel, { 0.0f, 5.0f, 0.0f });      // 逃走（オレンジ）
	enemyManager_->CreateEnemy(EnemyType::ClusterSlime, { 5.0f, 8.0f, -5.0f }); // 群れ（紫）

	// プレイヤーにエネミーマネージャーを渡して、プレイヤーからエネミーを参照できるようにする
	if (player_) {
		player_->SetEnemyManager(enemyManager_.get());
	}

	debugGrid_ = std::make_unique<DebugGrid>();
	debugGrid_->Initialize(DirectXCommon::GetInstance());

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());
}

void GamePlayScene::Finalize() {
	// 終了処理の実体を追加
}

void GamePlayScene::Update() {

#ifdef USE_IMGUI
	ImGui::Begin("a");
	// Well object debug / info
	if (wellObject_) {
		ImGui::Separator();
		ImGui::Text("Well Object");

		Vector3 wpos = wellObject_->GetTranslate();
		if (ImGui::DragFloat3("Well Position", &wpos.x, 0.1f)) {
			wellObject_->SetTranslate(wpos);
		}

		Vector3 wscale = wellObject_->GetScale();
		if (ImGui::DragFloat3("Well Scale", &wscale.x, 0.001f, 0.0001f, 100.0f)) {
			wellObject_->SetScale(wscale);
		}

		Vector3 wrot = wellObject_->GetRotate();
		ImGui::Text("Rotation: %.3f, %.3f, %.3f", wrot.x, wrot.y, wrot.z);

		if (ImGui::Button("Reset Well")) {
			wellObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
			wellObject_->SetScale({ 1.0f, 1.0f, 1.0f });
		}

	} else {
		ImGui::Separator();
		ImGui::Text("Well: not created");
	}

	player_->DrawImGui();

	ImGui::End();

#endif
	stageEditor_->Update();

	// Debug overlay to help diagnose warp issues (rendered inside ImGui frame)
#ifdef USE_IMGUI
	ImGui::Begin("Warp Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
	ImGui::Text("EditMode: %s", stageEditor_ && stageEditor_->IsEditMode() ? "ON" : "OFF");
	int warpCount = 0;
	for (const auto& o : stage_->GetStageData().objects)
		if (o.blockId == BlockID::Warp)
			++warpCount;
	ImGui::Text("Warp objects: %d", warpCount);
	ImGui::Text("LastWarpId: %d", lastWarpId_);
	ImGui::Text("WarpCooldown: %d", warpCooldownCounter_);
	ImGui::End();
#endif

	// Advance stage runtime (moving platforms, etc.) with fixed timestep
	if (stage_) {
		stage_->Update(1.0f / 60.0f);
		// Apply platform movement deltas to any player standing on them
		auto deltas = stage_->ConsumePlatformDeltas();
		if (player_) {
			Vector3 ppos = player_->GetPosition();
			bool applied = false;
			// assume player's half-height ~1.0f (matches Player::colliderHalfSize_ default)
			const float playerHalfY = 1.0f;

			for (const auto& o : stage_->GetStageData().objects) {
				if (o.blockId != BlockID::MovingPlatform)
					continue;
				auto it = deltas.find(o.id);
				if (it == deltas.end())
					continue;
				const Vector3 delta = it->second;

				// compute platform top Y using half-height = o.scale.y
				float topY = o.position.y + o.scale.y;
				float playerBottom = ppos.y - playerHalfY;

				const float kVertEps = 0.25f; // tolerance for standing on top
				// horizontal extents (use o.scale as half extents)
				float halfX = o.scale.x;
				float halfZ = o.scale.z;

				if (std::fabs(playerBottom - topY) <= kVertEps && std::fabs(ppos.x - o.position.x) <= halfX + 0.5f && std::fabs(ppos.z - o.position.z) <= halfZ + 0.5f) {
					// Apply full delta (or only horizontal components depending on direction)
					Vector3 applyDelta = { 0.0f, 0.0f, 0.0f };
					if (o.moveDirection == 3 || o.moveDirection == 4) {
						applyDelta.x = delta.x;
						applyDelta.z = delta.z;
					} else if (o.moveDirection == 1 || o.moveDirection == 2) {
						applyDelta.y = delta.y;
					} else {
						applyDelta = delta;
					}
					player_->SetRidingPlatformDelta(applyDelta);
					Logger::Log(
						std::string("Platform carry apply id:") + std::to_string(o.id) + " delta:" + std::to_string(delta.x) + "," + std::to_string(delta.y) + "," + std::to_string(delta.z) +
						" apply:" + std::to_string(applyDelta.x) + "," + std::to_string(applyDelta.y) + "," + std::to_string(applyDelta.z) + " playerPos:" + std::to_string(ppos.x) + "," +
						std::to_string(ppos.y) + "," + std::to_string(ppos.z) + "\n");
					applied = true;
					break;
				}
			}
			if (!applied)
				player_->ClearRidingPlatformDelta();
		}
	}

	// 実行時ステージから必要情報を取得
	stageBlockColliders_ = stage_->GetBlockOBBs();
	waterBlockColliders_ = stage_->GetWaterBlockOBBs();
	player_->SetBlockColliders(&stageBlockColliders_);
	cameraController_->SetObstacleColliders(&stageBlockColliders_);
	if (reticle_) {
		reticle_->Update();

		if (player_) {
			if (reticle_->HasAimTargetPoint()) {
				player_->SetAimTargetPoint(reticle_->GetAimTargetPoint());
			} else {
				player_->ClearAimTargetPoint();
			}
		}
	}



	// 【修正：追加】エネミーマネージャーに当たり判定データを渡す
	if (enemyManager_) {
		enemyManager_->SetBlockColliders(&stageBlockColliders_);
	}

	if (!stageEditor_->IsEditMode()) {
		cameraController_->Update(player_->GetPosition());

		// 【追加】ここ！ プレイヤーを Update する直前に減速倍率をセットする
		if (enemyManager_ && player_) {
			float totalSlowdown = enemyManager_->GetTotalPlayerSpeedMultiplier();
			player_->SetSpeedMultiplier(totalSlowdown);
		}

		player_->Update();

		if (enemyManager_) {
			enemyManager_->Update(1.0f / 60.0f, player_->GetPosition());
		}
	}

	// Warp detection: run before player update so teleport is immediate in gameplay mode
	if (player_) {
		const CollisionUtility::OBB playerObb = player_->GetPlayerOBB(player_->GetPosition());
		for (const auto& o : stage_->GetStageData().objects) {
			if (o.blockId != BlockID::Warp)
				continue;

			Transform t;
			t.translate = o.position;
			t.rotate = o.rotation;
			t.scale = o.scale;

			// Inflate warp OBB a bit to make triggering more forgiving
			const float warpInflation = 1.5f;
			CollisionUtility::OBB obb = CollisionUtility::MakeOBBFromTransform(t, { 1.0f * o.scale.x * warpInflation, 1.0f * o.scale.y * warpInflation, 1.0f * o.scale.z * warpInflation });
			std::string msg = "Checking warp id:" + std::to_string(o.id) + " pos:" + std::to_string(o.position.x) + "," + std::to_string(o.position.y) + "," + std::to_string(o.position.z) + "\n";
			Logger::Log(msg);

			if (CollisionUtility::IntersectOBB_OBB(playerObb, obb)) {
				std::string hitmsg = "Player intersects warp id:" + std::to_string(o.id) + "\n";
				Logger::Log(hitmsg);

				if (warpCooldownCounter_ == 0 || lastWarpId_ != o.id) {
					// Request teleport to be applied inside the player's
					// own Update to avoid being overwritten by other systems.
					player_->SetPendingTeleport(o.warpTargetPosition);
					lastWarpId_ = o.id;
					// set a cooldown to avoid immediate re-trigger loops
					warpCooldownCounter_ = 90; // frames (~1.5 seconds at 60fps)
					Logger::Log(
						std::string("Warped to ") + std::to_string(o.warpTargetPosition.x) + "," + std::to_string(o.warpTargetPosition.y) + "," + std::to_string(o.warpTargetPosition.z) + "\n");
				} else {
					Logger::Log(std::string("Warp ignored due to cooldown. id:") + std::to_string(o.id) + " counter:" + std::to_string(warpCooldownCounter_) + "\n");
				}
				break;
			}
		}

		if (warpCooldownCounter_ > 0)
			--warpCooldownCounter_;
		else
			lastWarpId_ = -1;
	}

	if (slug_) {
		slug_->Update(1.0f / 60.0f);
	}

	player_->UpdateTransparencyByCamera(camera_->GetTranslate());

	// 虫の更新
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

		// Tongue hitting stage blocks (apply damage to breakable blocks)
		if (tongue) {
			const CollisionUtility::Sphere tongueSphere = tongue->GetHitSphere();
			for (const auto& obb : stageBlockColliders_) {
				if (CollisionUtility::IntersectSphere_OBB(tongueSphere, obb)) {
					// apply damage (1) at tongue sphere
					stage_->ApplyDamageAtSphere(tongueSphere, 1);
					tongue->Reset();
					break;
				}
			}
		}
	}

	// 水ブロックに触れている間は徐々に回復
	bool isTouchingWater = false;
	if (player_) {
		const CollisionUtility::OBB playerObb = player_->GetPlayerOBB(player_->GetPosition());
		for (const auto& waterBox : waterBlockColliders_) {
			if (CollisionUtility::IntersectOBB_OBB(playerObb, waterBox)) {
				isTouchingWater = true;
				break;
			}
		}
	}

	if (isTouchingWater) {
		player_->AddWater(15.0f / 60.0f);
	}

	// (Warp detection handled earlier before player update.)


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
	if (wellObject_) {
		wellObject_->Update();
	}
	// DebugRenderer::GetInstance()->AddGrid({ 0.0f,0.0f,0.0f }, 5.0f, 10, { 0.0f,0.0f,0.0f,1.0f });
	DebugRenderer::GetInstance()->AddBox3DSolid({ 0.0f, 1.5f, 0.0f }, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f, 1.0f });

}

void GamePlayScene::Draw() {
	skybox_->Draw(*camera_);

	if (wellObject_) {
		wellObject_->Draw();
	}

	object3d_->Draw();
	// --- 不透明オブジェクトの描画 ---
	stage_->Draw();
	stageEditor_->Draw();
	player_->Draw();
	for (auto& bug : bugs_) {
		bug->Draw();
	}

	if (slug_) {
		slug_->Draw();
	}

	// 【追加】エネミーの描画
	if (enemyManager_) {
		enemyManager_->Draw();
	}

	// 半透明
	if (slug_) {
		slug_->DrawTransparent(*camera_);
	}

	debugGrid_->Draw(*camera_);
	
	SpriteCommon::GetInstance()->CommonDrawSetting();
	if (reticle_) {
		reticle_->Draw();
	}

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