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
#include "Player.h"
#include "CameraController.h"
#include "StageEdit.h"

#include <imgui.h>
#include <numbers>

void GamePlayScene::Initialize() {
	camera_ = std::make_unique<Camera>();
	camera_->InitializeGPU(DirectXCommon::GetInstance()->GetDevice());
	//camera_->SetRotate({ 0.3f,0.0f,0.0f });
	//camera_->SetTranslate({ 0.0f,4.0f,-10.0f });
	//camera_->SetRotate({ 0.0f,0.0f,0.0f });
	//camera_->SetTranslate({ 0.0f,0.0f,-10.0f });
	camera_->SetRotate({ std::numbers::pi_v<float> / 10.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,7.5f,-20.0f });

	//object3dCommon_->SetDefaultCamera(camera_);

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
	ModelManager::GetInstance()->LoadModel("Kanban1.obj");
	ModelManager::GetInstance()->LoadModel("Cube.obj");

	se_ = SoundManager::GetInstance()->LoadFile("resources/mokugyo.wav");
	//SoundManager::GetInstance()->PlayerWave(se_);

	object3d_ = std::make_unique<Object3d>();
	object3d_->Initialize(Object3dCommon::GetInstance());
	object3d_->SetModel("plane.gltf");
	object3d_->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d_->SetCamera(camera_.get());

	player_ = std::make_unique<Player>();
	player_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Cube.obj", { 0.0f, 0.0f, 0.0f });
	player_->SetGroundHeight(0.0f);

	cameraController_ = std::make_unique<CameraController>();
	cameraController_->Initialize(camera_.get());
	cameraController_->SetTargetOffset({ 0.0f, 1.0f, 0.0f });
	cameraController_->SetDistance(25.0f);
	cameraController_->SetHeight(1.5f);
	cameraController_->SetYawSpeed(0.03f);
	cameraController_->SetPitchSpeed(0.02f);

	stageEdit_ = std::make_unique<StageEdit>();
	stageEdit_->Initialize(Object3dCommon::GetInstance(), camera_.get(), "Cube.obj");

	monsterBall_ = std::make_unique<Object3d>();
	monsterBall_->Initialize(Object3dCommon::GetInstance());
	monsterBall_->SetCamera(camera_.get());
	monsterBall_->SetModel("sphere.obj");
	monsterBall_->SetRotate({ 0.0f, -std::numbers::pi_v<float> / 2.0f, 0.0f });

	terrain_ = std::make_unique<Object3d>();
	terrain_->Initialize(Object3dCommon::GetInstance());
	terrain_->SetModel("terrain.obj");
	terrain_->SetCamera(camera_.get());
	terrain_->SetTranslate({ 0.0f,0.0f,0.0f });
	terrain_->SetRotate({ 0.0f,-std::numbers::pi_v<float> / 2.0f, 0.0f });

	pointLight_.color = { 1.0f,1.0f,1.0f,1.0f };
	pointLight_.position = { 0.0f,2.0f,0.0f };
	pointLight_.intensity = 1.0f;
	pointLight_.radius = 3.0f;
	pointLight_.decay = 1.0f;

	Object3dCommon::GetInstance()->SetPointLight(pointLight_);

	spotLight_.color = { 1.0f,1.0f,1.0f,1.0f };
	spotLight_.position = { 2.0f,1.25f,0.0f };
	spotLight_.distance = 7.0f;
	spotLight_.direction = Vector3::Normalized({ -1.0f,-1.0f,0.0f });
	spotLight_.intensity = 4.0f;
	spotLight_.decay = 2.0f;
	spotLight_.cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);
	spotLight_.cosFalloffStart = std::cos(std::numbers::pi_v<float> / 18.0f);

	Object3dCommon::GetInstance()->SetSpotLight(spotLight_);

	//particleManager_->CreateParticleGroup("test", "resources/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");
	testParticle_ = std::make_unique<ParticleEmitter>(DirectXCommon::GetInstance(), SrvManager::GetInstance(), camera_.get());
	testParticle_->transform_ = {};
	testParticle_->name_ = "test";
	testParticle_->count_ = 20;
	testParticle_->frequencyTime_ = 1.0f;
	testParticle_->config_.minScale = { 0.05f,0.4f,1.0f };
	testParticle_->config_.maxScale = { 0.05f,1.5f,1.0f };
	testParticle_->config_.minRotate = { 0.0f,0.0f,-std::numbers::pi_v<float> };
	testParticle_->config_.maxRotate = { 0.0f,0.0f,std::numbers::pi_v<float> };
	testParticle_->config_.minVelocity = {};
	testParticle_->config_.maxVelocity = {};
	testParticle_->config_.lifeTime = 1.0f;

	RingManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	RingManager::GetInstance()->SetCamera(camera_.get());

	CylinderManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());
	CylinderManager::GetInstance()->SetCamera(camera_.get());

	for (uint32_t i = 0; i < kMaxSprite; ++i) {
		std::unique_ptr<Sprite> sprite = std::make_unique<Sprite>();
		std::string texture = "resources/uvChecker.png";
		if (i % 2 == 0) {
			texture = "resources/uvChecker.png";
		} else {
			texture = "resources/monsterBall.png";
		}
		sprite->Initialize(SpriteCommon::GetInstance(), texture);
		sprite->SetPos({ i * 200.0f,0.0f });
		sprite->SetSize({ 100.0f,100.0f });
		sprite->SetTextureLeftTop({ 0.0f,0.0f });
		sprite->SetTextureSize({ 256.0f,256.0f });
		sprites_.push_back(std::move(sprite));
	}

	std::string testTexture = "resources/monsterBall.png";
	testSprite_ = std::make_unique<Sprite>();
	testSprite_->Initialize(SpriteCommon::GetInstance(), testTexture);
	testSprite_->SetPos({ 100.0f,100.0f });
	testSprite_->SetSize({ 500.0f,500.0f });
	testSprite_->SetAnchorPoint({ 0.0f,0.0f });
	testSprite_->SetTextureSize({ 1200.0f,600.0f });

	  // 虫の生成と初期化
	bug_ = std::make_unique<Bug>();
	bug_->Initialize(camera_.get());

}

void GamePlayScene::Finalize() {
}

void GamePlayScene::Update() {
	imGuiManager_->Begin();
#ifdef USE_IMGUI
	// デモウィンドウ(使い方紹介)
	ImGui::ShowDemoWindow();

	Vector3 rotate = object3d_->GetRotate();

	Vector4 lightColor = monsterBall_->GetLightColor();
	Vector3 lightDirection = monsterBall_->GetLightDirection();
	float lightIntensity = monsterBall_->GetLightIntensity();
	Vector2 testSpritePos = testSprite_->GetPos();

	ImGui::Begin("Window");

	camera_->DrawImGui();
	if (ImGui::TreeNode("object3d")) {
		ImGui::DragFloat3("rotate", &rotate.x, 0.01f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("MonsterBallLight")) {
		ImGui::DragFloat4("LightColor", &lightColor.x, 0.01f);
		ImGui::DragFloat3("LightDirection", &lightDirection.x, 0.01f);
		ImGui::DragFloat("LightIntensity", &lightIntensity, 0.01f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("PointLight")) {
		ImGui::DragFloat3("Pos", &pointLight_.position.x, 0.01f);
		ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.01f);
		ImGui::DragFloat4("color", &pointLight_.color.x, 0.01f);
		ImGui::DragFloat("radius", &pointLight_.radius, 0.01f);
		ImGui::DragFloat("decay", &pointLight_.decay, 0.01f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("SpotLight")) {
		ImGui::DragFloat3("Pos", &spotLight_.position.x, 0.01f);
		ImGui::DragFloat("Intensity", &spotLight_.intensity, 0.01f);
		ImGui::DragFloat4("color", &spotLight_.color.x, 0.01f);
		ImGui::DragFloat("distance", &spotLight_.distance, 0.01f);
		ImGui::DragFloat("decay", &spotLight_.decay, 0.01f);
		ImGui::DragFloat("cosAngle", &spotLight_.cosAngle, 0.01f);      // なんかできない
		ImGui::DragFloat3("direction", &spotLight_.direction.x, 0.01f); // なんかできない
		ImGui::DragFloat("cosFalloffStart", &spotLight_.cosFalloffStart, 0.01f);
		ImGui::TreePop();
	}
	if (ImGui::TreeNode("Sprite")) {
		ImGui::DragFloat2("position", &testSpritePos.x, 1.0f, 0.0f, 0.0f, "%4.3f");
		ImGui::TreePop();
	}

	ImGui::End();


	object3d_->SetRotate(rotate);

	monsterBall_->SetLightColor(lightColor);
	monsterBall_->SetLightDirection(lightDirection);
	monsterBall_->SetLightIntensity(lightIntensity);

	Object3dCommon::GetInstance()->SetPointLight(pointLight_);
	Object3dCommon::GetInstance()->SetSpotLight(spotLight_);

	testSprite_->SetPos(testSpritePos);
#endif
	
	// デバッグ用: チャージストック変更
	if(Input::GetInstance()->IsTriggerKey(DIK_1)){
		player_->SetChargeStock(0);
	}
	if(Input::GetInstance()->IsTriggerKey(DIK_2)){
		player_->SetChargeStock(1);
	}
	if(Input::GetInstance()->IsTriggerKey(DIK_3)){
		player_->SetChargeStock(2);
	}
	if(Input::GetInstance()->IsTriggerKey(DIK_4)){
		player_->SetChargeStock(3);
	}

	stageEdit_->Update();

	if(!stageEdit_->IsEditMode()){
		// いつもの更新
		player_->Update(cameraController_->GetYaw());
		cameraController_->Update(player_->GetPosition());

	} else{
		// StageEdit中はプレイヤー更新を止める
		cameraController_->Update(player_->GetPosition());
	}

	imGuiManager_->End();



	// 虫の更新
	bug_->Update();

	camera_->Update();
	camera_->TransferToGPU();

	object3d_->Update();

	monsterBall_->Update();

	terrain_->Update();

	testParticle_->Update(1.0f / 60.0f);

	ParticleManager::GetInstance()->Update(1.0f / 60.0f); // すべてのパーティクルの更新

	RingConfig ring;
	ring.isBillboard = true;
	ring.startScale = 1.0f;
	ring.endScale = 10.0f;
	ring.lifeTime = 1.0f;
	ring.startScale = 0.5f;
	ring.endScale = 0.5f;

	if(Input::GetInstance()->IsTriggerMouse(1)){
		RingManager::GetInstance()->Emit({}, {}, ring);
	}

	RingManager::GetInstance()->Update(1.0f / 60.0f);

	CylinderConfig cylinder;
	cylinder.startScale = 1.0f;
	cylinder.endScale = 1.0f;
	cylinder.lifeTime = 1.0f;
	if (Input::GetInstance()->IsTriggerMouse(0)) {
		CylinderManager::GetInstance()->Emit({}, cylinder);
	}
	CylinderManager::GetInstance()->Update(1.0f / 60.0f);


	for (uint32_t i = 0; i < sprites_.size(); ++i) {
		sprites_[i]->Update();
	}

	testSprite_->Update();
}

void GamePlayScene::Draw() {
	// 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
	Object3dCommon::GetInstance()->CommonDrawSetting();

	//object3d_->Draw();
	player_->Draw();
	stageEdit_->Draw();

	// 虫の描画
	bug_->Draw();

	//monsterBall_->Draw();

	//terrain_->Draw();

	//ParticleManager::GetInstance()->Draw();

	RingManager::GetInstance()->Draw();

	CylinderManager::GetInstance()->Draw();

	SpriteCommon::GetInstance()->CommonDrawSetting();

	for (const std::unique_ptr<Sprite>& sprite : sprites_) {
		//sprite->Draw();
	}

	//testSprite_->Draw();

	imGuiManager_->Draw();
}

GamePlayScene::GamePlayScene() = default;

GamePlayScene::~GamePlayScene() = default;