#include "Game.h"

#include "TextureManager.h"
#include "ModelManager.h"
#include "ImGuiManager.h"
#include "ParticleManager.h"
#include "engine/audio/SoundManager.h"
#include "engine/3d/DebugCamera.h"
#include "Object3dCommon.h"
#include "SpriteCommon.h"

#include "Logger.h"

#include "Sprite.h"
#include "Object3d.h"
#include "ParticleEmitter.h"

#include <imgui.h>
#include <numbers>

void Game::Initialize() {

	Framework::Initialize();

	camera_ = new Camera();
	camera_->InitializeGPU(GetDxCommon()->GetDevice());
	camera_->SetRotate({ 0.3f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,4.0f,-10.0f });
	camera_->SetRotate({ 0.0f,0.0f,0.0f });
	camera_->SetTranslate({ 0.0f,0.0f,-10.0f });

	//object3dCommon_->SetDefaultCamera(camera_);

	imGuiManager_ = new ImGuiManager();
	imGuiManager_->Initialize(GetWinApp(), GetDxCommon(), GetSrvManager());

	ParticleManager::GetInstance()->Initialize(GetDxCommon(), GetSrvManager());
	ParticleManager::GetInstance()->SetCamera(camera_);

	debugCamera_ = new DebugCamera();
	debugCamera_->Initialize();

	// TextureManager からテクスチャを読み込む
	TextureManager::GetInstance()->LoadTexture("resources/uvChecker.png");
	TextureManager::GetInstance()->LoadTexture("resources/monsterBall.png");

	// .objファイルからモデルを読み込む
	ModelManager::GetInstance()->LoadModel("plane.obj");
	ModelManager::GetInstance()->LoadModel("sphere.obj");

	se_ = SoundManager::GetInstance()->LoadFile("resources/mokugyo.wav");
	SoundManager::GetInstance()->PlayerWave(se_);

	object3d_ = new Object3d();
	object3d_->Initialize(GetObject3dCommon());
	object3d_->SetModel("plane.obj");
	object3d_->SetTranslate({ 0.0f, 0.0f, 5.0f });
	object3d_->SetRotate({ 0.0f, std::numbers::pi_v<float>, 0.0f });
	object3d_->SetCamera(camera_);

	monsterBall_ = new Object3d();
	monsterBall_->Initialize(GetObject3dCommon());
	monsterBall_->SetCamera(camera_);
	monsterBall_->SetModel("sphere.obj");
	monsterBall_->SetRotate({ 0.0f, -std::numbers::pi_v<float> / 2.0f, 0.0f });

	//particleManager_->CreateParticleGroup("test", "resources/circle.png");
	ParticleManager::GetInstance()->CreateParticleGroup("test", "resources/circle.png");
	testParticle_ = new ParticleEmitter(GetDxCommon(), GetSrvManager(), camera_);
	testParticle_->transform_ = {};
	testParticle_->name_ = "test";
	testParticle_->count_ = 10;
	testParticle_->frequencyTime_ = 1.0f;

	for (uint32_t i = 0; i < kMaxSprite; ++i) {
		Sprite* sprite = new Sprite();
		std::string texture = "resources/uvChecker.png";
		if (i % 2 == 0) {
			texture = "resources/uvChecker.png";
		} else {
			texture = "resources/monsterBall.png";
		}
		sprite->Initialize(GetSpriteCommon(), texture);
		sprite->SetPos({ i * 200.0f,0.0f });
		sprite->SetSize({ 100.0f,100.0f });
		sprite->SetTextureLeftTop({ 0.0f,0.0f });
		sprite->SetTextureSize({ 256.0f,256.0f });
		sprites_.push_back(sprite);
	}

	std::string testTexture = "resources/monsterBall.png";
	testSprite_ = new Sprite();
	testSprite_->Initialize(GetSpriteCommon(), testTexture);
	testSprite_->SetPos({ 100.0f,100.0f });
	testSprite_->SetSize({ 500.0f,500.0f });
	testSprite_->SetAnchorPoint({ 0.0f,0.0f });
	testSprite_->SetTextureSize({ 1200.0f,600.0f });

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");
}

void Game::Finalize() {

	delete testSprite_;
	testSprite_ = nullptr;

	for (Sprite* sprite : sprites_) {
		delete sprite;
	}
	sprites_.clear();

	delete testParticle_;
	testParticle_ = nullptr;

	delete monsterBall_;
	monsterBall_ = nullptr;

	delete object3d_;
	object3d_ = nullptr;

	delete debugCamera_;
	debugCamera_ = nullptr;

	//ParticleManager::GetInstance()->Finalize();

	imGuiManager_->Finalize();
	delete imGuiManager_;
	imGuiManager_ = nullptr;

	delete camera_;
	camera_ = nullptr;

	Framework::Finalize();
}

void Game::Update() {
	Framework::Update();

	imGuiManager_->Begin();
#ifdef USE_IMGUI
	// デモウィンドウ(使い方紹介)
	ImGui::ShowDemoWindow();

	Vector2 testSpritePos = testSprite_->GetPos();

	ImGui::Begin("Window");

	camera_->DrawImGui();

	if (ImGui::TreeNode("Sprite")) {
		ImGui::DragFloat2("position", &testSpritePos.x, 1.0f, 0.0f, 0.0f, "%4.3f");
		ImGui::TreePop();
	}

	ImGui::End();

	imGuiManager_->End();

	testSprite_->SetPos(testSpritePos);
#endif

	camera_->Update();
	camera_->TransferToGPU();

	object3d_->Update();

	monsterBall_->Update();

	testParticle_->Update(1.0f / 60.0f);

	ParticleManager::GetInstance()->Update(1.0f / 60.0f); // すべてのパーティクルの更新

	for (uint32_t i = 0; i < sprites_.size(); ++i) {
		sprites_[i]->Update();
	}

	testSprite_->Update();
}

void Game::Draw() {

	Framework::Draw();

	// 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
	GetObject3dCommon()->CommonDrawSetting();

	object3d_->Draw();

	monsterBall_->Draw();

	ParticleManager::GetInstance()->Draw();

	GetSpriteCommon()->CommonDrawSetting();

	for (Sprite* sprite : sprites_) {
		sprite->Draw();
	}

	testSprite_->Draw();

	imGuiManager_->Draw();

}