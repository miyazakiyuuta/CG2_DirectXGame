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
#include "StageSerializer.h"
#include "effect/ParticleEmitter.h"
#include "3d/Skybox.h"
#include "debug/DebugGrid.h"
#include "Tongue.h"
#include "debug/DebugGrid.h"
#include "effect/ParticleEmitter.h"

#include <numbers>
#ifdef USE_IMGUI
#include <imgui.h>
#endif
#include "utility/Logger.h"
#include <sstream>

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
    //object3d_->SetModel("Frog.gltf");
    object3d_->SetCamera(camera_.get());
    object3d_->SetTranslate({ 0.0f, 0.0f, 5.0f });
    object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
    object3d_->SetColor({ 0.5f,0.5f,0.5f,1.0f });
    object3d_->SetUseEnvironmentMap(true); // 環境マップ

    // Create the well object and place it at a fixed position only if model is loaded
    if(Object3dCommon::GetInstance() && camera_){
        // Ensure model was actually loaded
        if(ModelManager::GetInstance()->FindModel("well.obj")){
            wellObject_ = std::make_unique<Object3d>();
            wellObject_->Initialize(Object3dCommon::GetInstance());
            wellObject_->SetModel("well.obj");
            wellObject_->SetCamera(camera_.get());
            // Adjust this position as needed
            // Move the well slightly further from the camera and make it very small
            wellObject_->SetTranslate({ 0.0f, 0.0f, 0.0f });
            wellObject_->SetScale({ 60.0f, 60.0f, 60.0f });
            wellObject_->SetColor({ 1.0f, 1.0f, 1.0f, 1.0f });
        } else{
            // Model not found; skip creating wellObject_
            wellObject_.reset();
        }
    } else{
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
    if(loadedStage){
        stage_->SetStageData(*loadedStage);
    }

    // プレイヤー開始位置を Stage から取得
    Vector3 playerStart = { 0.0f, 3.0f, 0.0f };
    if(auto spawn = stage_->GetPlayerSpawnPosition()){
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

    player_->SetCameraController(cameraController_.get());

    // StageEditor は Stage を受け取って編集するだけ
    stageEditor_ = std::make_unique<StageEditor>(stage_.get(), Object3dCommon::GetInstance(), camera_.get());
    stageEditor_->Initialize("Cube.obj");

    // 虫の生成と初期化
    bugs_.clear();
    for(const auto& spawnPos : stage_->GetBugSpawnPositions()){
        auto bug = std::make_unique<Bug>();
        bug->Initialize(camera_.get());
        bug->SetPositionImmediate(spawnPos);
        bugs_.push_back(std::move(bug));
    }

    // ナメクジの初期化
    slug_ = std::make_unique<Slug>();
    slug_->Initialize(
        DirectXCommon::GetInstance(),
        Object3dCommon::GetInstance(), camera_.get(), "sphere.obj");
    slug_->SetPosition({ 5.0f, 0.5f, 5.0f });
    slug_->SetPosition({ 5.0f, 1.2f, 5.0f });

    slug_->SetBodyColor({ 0.8f, 0.2f, 0.2f, 1.0f });
    slug_->SetTrailColor({ 1.0f, 0.0f, 1.0f, 1.0f });

    debugGrid_ = std::make_unique<DebugGrid>();
    debugGrid_->Initialize(DirectXCommon::GetInstance());
}

void GamePlayScene::Finalize(){
    // 終了処理の実体を追加
}

void GamePlayScene::Update(){
#ifdef USE_IMGUI
    imGuiManager_->Begin();

    ImGui::ShowDemoWindow();

    Vector3 rotate = object3d_->GetRotate();

    ImGui::Begin("Window");

    camera_->DrawImGui();

    // Well object debug / info
    if(wellObject_){
        ImGui::Separator();
        ImGui::Text("Well Object");

        Vector3 wpos = wellObject_->GetTranslate();
        if(ImGui::DragFloat3("Well Position", &wpos.x, 0.1f)){
            wellObject_->SetTranslate(wpos);
        }

        Vector3 wscale = wellObject_->GetScale();
        if(ImGui::DragFloat3("Well Scale", &wscale.x, 0.001f, 0.0001f, 100.0f)){
            wellObject_->SetScale(wscale);
        }

        Vector3 wrot = wellObject_->GetRotate();
        ImGui::Text("Rotation: %.3f, %.3f, %.3f", wrot.x, wrot.y, wrot.z);

        if(ImGui::Button("Reset Well")){
            wellObject_->SetTranslate({ 0.0f,0.0f,0.0f });
            wellObject_->SetScale({ 1.0f,1.0f,1.0f });
        }

    } else{
        ImGui::Separator();
        ImGui::Text("Well: not created");
    }

    if(ImGui::TreeNode("object3d")){
        ImGui::DragFloat3("rotate", &rotate.x, 0.01f);
        ImGui::TreePop();
    }

    player_->DrawImGui();

    ImGui::End();

    stageEditor_->Update();

    imGuiManager_->End();
#endif

    // 実行時ステージから必要情報を取得
    stageBlockColliders_ = stage_->GetBlockOBBs();
    waterBlockColliders_ = stage_->GetWaterBlockOBBs();
    player_->SetBlockColliders(&stageBlockColliders_);
    cameraController_->SetObstacleColliders(&stageBlockColliders_);

    if(!stageEditor_->IsEditMode()){
        // 先にカメラを更新して、そのフレームの aim 状態と forward を Player が読めるようにする
        cameraController_->Update(player_->GetPosition());
        player_->Update();
    } else{
        // StageEditor中はプレイヤー更新を止める
        cameraController_->Update(player_->GetPosition());
    }

    if(slug_){
        slug_->Update(1.0f / 60.0f);
    }

    player_->UpdateTransparencyByCamera(camera_->GetTranslate());

    // 虫の更新
    for(auto& bug : bugs_){
        bug->Update();
    }

    if(player_){
        Tongue* tongue = player_->GetTongue();
        if(tongue && tongue->CanHitBug()){
            const CollisionUtility::Sphere tongueSphere = tongue->GetHitSphere();
            for(auto& bug : bugs_){
                if(CollisionUtility::IntersectSphere_Sphere(tongueSphere, bug->GetHitSphere())){
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
        const CollisionUtility::OBB playerObb = player_->GetPlayerOBB(player_->GetPosition());
        for(const auto& waterBox : waterBlockColliders_){
            if(CollisionUtility::IntersectOBB_OBB(playerObb, waterBox)){
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

    if(Input::GetInstance()->IsTriggerKey(DIK_0)){
        object3d_->StopAnimation();
    }
    if(Input::GetInstance()->IsTriggerKey(DIK_9)){
        object3d_->PauseSwitchingAnimation();
    }
    if(Input::GetInstance()->IsPushKey(DIK_1)){
        object3d_->PlayAnimation("walk", false, 1.0f);
    }
    if(Input::GetInstance()->IsTriggerKey(DIK_2)){
        object3d_->PlayAnimation("sneakWalk", true, 1.0f);
    }

    object3d_->Update();

    if(wellObject_){
        wellObject_->Update();
    }
}

void GamePlayScene::Draw(){
    skybox_->Draw(*camera_);

    if(wellObject_){
        wellObject_->Draw();
    }

    object3d_->Draw();

    // --- 不透明オブジェクトの描画 ---
    stage_->Draw();
    stageEditor_->Draw();
    player_->Draw();
    for(auto& bug : bugs_){
        bug->Draw();
    }

    if(slug_){
        slug_->Draw();
    }

    // 半透明
    if(slug_){
        slug_->DrawTransparent(*camera_);
    }

    debugGrid_->Draw(*camera_);

#ifdef USE_IMGUI
    imGuiManager_->Draw();
#endif
}

GamePlayScene::GamePlayScene() = default;
GamePlayScene::~GamePlayScene() = default;