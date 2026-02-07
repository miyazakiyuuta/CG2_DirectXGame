#include "Framework.h"
#include "WinApp.h"
#include "Input.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "engine/audio/SoundManager.h"
#include "SpriteCommon.h"
#include "Object3dCommon.h"
#include "Logger.h"

void Framework::Initialize() {
	winApp_ = new WinApp();
	winApp_->Initialize();

	Input::GetInstance()->Initialize(winApp_);

	dxCommon_ = new DirectXCommon();
	dxCommon_->Initialize(winApp_);

	srvManager_ = new SrvManager();
	srvManager_->Initialize(dxCommon_);

	TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

	ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);

	SoundManager::GetInstance()->Initialize();

	// 3Dオブジェクト共通部の初期化
	object3dCommon_ = new Object3dCommon();
	object3dCommon_->Initialize(dxCommon_);

	// スプライト共通部の初期化
	spriteCommon_ = new SpriteCommon;
	spriteCommon_->Initialize(dxCommon_, srvManager_);

	Logger::Initialize();
}

void Framework::Finalize() {
	delete spriteCommon_;
	spriteCommon_ = nullptr;
	delete object3dCommon_;
	object3dCommon_ = nullptr;
	SoundManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	delete srvManager_;
	srvManager_ = nullptr;
	delete dxCommon_;
	dxCommon_ = nullptr;
	delete winApp_;
	winApp_ = nullptr;
	Logger::Finalize();
}

void Framework::Update() {
	if (winApp_->ProcessMessage()) {
		// ゲームループを抜ける
		isEndRequest_ = true;
	}

	Input::GetInstance()->Update();
}

void Framework::Draw() {
	// 3Dオブジェクトの描画準備。3Dオブジェクトの描画に共通のグラフィックスコマンドを積む
	//object3dCommon_->CommonDrawSetting();
	// スプライト描画
	//spriteCommon_->CommonDrawSetting();
}

void Framework::Run() {
	// ゲームの初期化
	Initialize();

	while (true) { // ゲームループ
		// 毎フレーム更新
		Update();
		// 終了要求が来たら抜ける
		if (IsEndRequest()) {
			break;
		}
		// 描画
		dxCommon_->PreDraw();
		srvManager_->PreDraw();
		Draw();
		dxCommon_->PostDraw();
	}
	// ゲームの終了
	Finalize();
}
