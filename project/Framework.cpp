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
	WinApp::GetInstance()->Initialize();

	Input::GetInstance()->Initialize(WinApp::GetInstance());

	DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());

	SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());

	TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	SoundManager::GetInstance()->Initialize();

	// 3Dオブジェクト共通部の初期化
	Object3dCommon::GetInstance()->Initialize(DirectXCommon::GetInstance());

	// スプライト共通部の初期化
	SpriteCommon::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	Logger::Initialize();
}

void Framework::Finalize() {
	
	SoundManager::GetInstance()->Finalize();
	ModelManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();
	
	Logger::Finalize();
}

void Framework::Update() {
	if (WinApp::GetInstance()->ProcessMessage()) {
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
		DirectXCommon::GetInstance()->PreDraw();
		SrvManager::GetInstance()->PreDraw();
		Draw();
		DirectXCommon::GetInstance()->PostDraw();
	}
	// ゲームの終了
	Finalize();
}
