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
#include "Player.h"
#include "CameraController.h"
#include "StageEditor.h"
#include "effect/ParticleEmitter.h"
#include "3d/Skybox.h"
#include "debug/DebugGrid.h"
#include "Tongue.h"

#include <imgui.h>
#include <numbers>

void GamePlayScene::Initialize(){
    camera_ = std::make_unique<Camera>();
    camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
    camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f, 0.0f, 0.0f });
    camera_->SetTranslate({ 0.0f, 7.5f, -20.0f });

    imGuiManager_ = std::make_unique<ImGuiManager>();
    imGuiManager_->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());

    ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
    ParticleManager::GetInstance()->SetCamera(camera_.get());

    debugCamera_ = std::make_unique<DebugCamera>();
    debugCamera_->Initialize();

    TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
    TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");
    TextureManager::GetInstance()->LoadTexture("resources/grass.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("plane.gltf");
	ModelManager::GetInstance()->LoadModel("sphere.obj");
	ModelManager::GetInstance()->LoadModel("terrain.obj");
	ModelManager::GetInstance()->LoadModel("human", "sneakWalk.gltf");
    ModelManager::GetInstance()->LoadModel("Kanban1.obj");
    ModelManager::GetInstance()->LoadModel("Cube.obj");

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("sneakWalk.gltf");
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

    // StageEditorの初期化
    stageEditor_ = std::make_unique<StageEditor>(Object3dCommon::GetInstance(), camera_.get());
    stageEditor_->Initialize("Cube.obj");

    // 起動時にステージJSONがあれば読み込んでオブジェクトを生成する
    // resources/stage.json を期待する
    stageEditor_->Load("resources/stage.json");

    // プレイヤー開始位置を StageEditor から取得
    Vector3 playerStart = { 0.0f, 3.0f, 0.0f };
    if(auto spawn = stageEditor_->GetPlayerSpawnPosition()){
        playerStart = *spawn;
    }

    player_ = std::make_unique<Player>();
    player_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Cube.obj", playerStart);

    cameraController_ = std::make_unique<CameraController>();
    cameraController_->Initialize(camera_.get());
    cameraController_->SetTargetOffset({ 0.0f, 1.0f, 0.0f });
    cameraController_->SetDistance(25.0f);
    cameraController_->SetHeight(1.5f);
    cameraController_->SetYawSpeed(0.03f);
    cameraController_->SetPitchSpeed(0.02f);
    cameraController_->SetObstacleColliders(&stageBlockColliders_);

    // 虫の生成と初期化
    bugs_.clear();
    for(const auto& spawnPos : stageEditor_->GetBugSpawnPositions()){
        auto bug = std::make_unique<Bug>();
        bug->Initialize(camera_.get());
        bug->SetPositionImmediate(spawnPos);
        bugs_.push_back(std::move(bug));
    }

    debugGrid_ = std::make_unique<DebugGrid>();
    debugGrid_->Initialize(DirectXCommon::GetInstance());
}

void GamePlayScene::Finalize(){}

void GamePlayScene::Update(){
    imGuiManager_->Begin();

#ifdef USE_IMGUI
    ImGui::ShowDemoWindow();

    Vector3 rotate = object3d_->GetRotate();

    ImGui::Begin("Window");

    camera_->DrawImGui();

    if(ImGui::TreeNode("object3d")){
        ImGui::DragFloat3("rotate", &rotate.x, 0.01f);
        ImGui::TreePop();
    }

    if(player_){
        player_->DrawImGui();
    }

    cameraController_->DrawImGui();

    ImGui::End();

    object3d_->SetRotate(rotate);
#endif

    stageEditor_->Update();

    // 通常ブロックと水ブロックを分けて取得
    stageBlockColliders_ = stageEditor_->GetBlockAABBs();
    waterBlockColliders_ = stageEditor_->GetWaterBlockAABBs();
    player_->SetBlockColliders(&stageBlockColliders_);

    if(!stageEditor_->IsEditMode()){
        // いつもの更新
        player_->Update(cameraController_->GetYaw());
        cameraController_->Update(player_->GetPosition());
    } else{
        // StageEditor中はプレイヤー更新を止める
        cameraController_->Update(player_->GetPosition());
    }

    player_->UpdateTransparencyByCamera(camera_->GetTranslate());

    imGuiManager_->End();

    // 虫の更新
    for(auto& bug : bugs_){
        bug->Update();
    }

    // 舌先と虫の球判定
    if(player_){
        Tongue* tongue = player_->GetTongue();
        if(tongue && tongue->CanHitBug()){
            const CollisionUtility::Sphere tongueSphere = tongue->GetHitSphere();

            for(auto& bug : bugs_){
                const CollisionUtility::Sphere bugSphere = bug->GetHitSphere();

                if(CollisionUtility::IntersectSphere_Sphere(tongueSphere, bugSphere)){
                    player_->AddChargeStock(1);
                    bug->OnTongueHit();
                    tongue->Reset();
                    break;
                }
            }
        }
    }

    // 水ブロックに触れている間は徐々に回復
    bool isTouchingWater = false;
    if(player_){
        const CollisionUtility::AABB playerBox = player_->GetPlayerAABB(player_->GetPosition());
        for(const auto& waterBox : waterBlockColliders_){
            if(CollisionUtility::IntersectAABB_AABB(playerBox, waterBox)){
                isTouchingWater = true;
                break;
            }
        }
    }

    if(isTouchingWater){
        player_->AddWater(15.0f / 60.0f);
    }

    camera_->Update();
    camera_->TransferToGPU();

	object3d_->Update();


}

void GamePlayScene::Draw(){

    skybox_->Draw(*camera_);

    object3d_->Draw();

    stageEditor_->Draw();

	player_->Draw();

    // 虫の描画
    for(auto& bug : bugs_){
        bug->Draw();
    }

	debugGrid_->Draw(*camera_);

	imGuiManager_->Draw();
}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;