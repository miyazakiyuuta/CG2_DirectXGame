#include "base/Framework.h"
#include "base/WinApp.h"
#include "base/DirectXCommon.h"
#include "base/SrvManager.h"
#include "io/Input.h"
#include "2d/TextureManager.h"
#include "2d/SpriteCommon.h"
#include "3d/ModelManager.h"
#include "3d/Object3dCommon.h"
#include "audio/SoundManager.h"
#include "utility/Logger.h"

void Framework::Initialize() {
	WinApp::GetInstance()->Initialize();

	Input::GetInstance()->Initialize(WinApp::GetInstance());

	DirectXCommon::GetInstance()->Initialize(WinApp::GetInstance());

	SrvManager::GetInstance()->Initialize(DirectXCommon::GetInstance());

	TextureManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	ModelManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	SoundManager::GetInstance()->Initialize();

	// 3Dオブジェクト共通部の初期化
	Object3dCommon::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	// スプライト共通部の初期化
	SpriteCommon::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

#ifdef USE_IMGUI
	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());

	auto dxCommon = DirectXCommon::GetInstance();
	auto renderTextureRtvHandle = dxCommon->GetRTVCPUDescriptorHandle(2);

	sceneRenderTarget_ = std::make_unique<RenderTarget>();
	//sceneRenderTarget_->Create(dxCommon->GetDevice(), SrvManager::GetInstance(), renderTextureRtvHandle, WinApp::kClientWidth, WinApp::kClientHeight);
	sceneRenderTarget_->Create(dxCommon->GetDevice(), SrvManager::GetInstance(), renderTextureRtvHandle, 1280, 720);

#endif

	Logger::Initialize();

}

void Framework::Finalize() {

#ifdef USE_IMGUI
	imGuiManager_->Finalize();
#endif
	
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

Framework::~Framework() = default;

void Framework::Run() {
	// ゲームの初期化
	Initialize();

	while (true) { // ゲームループ
#ifdef USE_IMGUI
		imGuiManager_->Begin();
#endif

		// 毎フレーム更新
		Update();
		// 終了要求が来たら抜ける
		if (IsEndRequest()) {
			break;
		}
		// 描画
#ifdef USE_IMGUI

		// リサイズ検知
		if (WinApp::IsResized()) {
			int w = WinApp::GetNewWidth();
			int h = WinApp::GetNewHeight();
			DirectXCommon::GetInstance()->ResizeSwapChain(w, h);
			WinApp::ClearResizedFlag();
		}
		
		SrvManager::GetInstance()->PreDraw();

		auto commandList = DirectXCommon::GetInstance()->GetCommandList();
		auto dsvHandle = DirectXCommon::GetInstance()->GetDSVCPUDescriptorHandle(0);

		sceneRenderTarget_->BeginRender(commandList, dsvHandle);
		Draw();
		sceneRenderTarget_->EndRender(commandList);

		DirectXCommon::GetInstance()->PreDraw();

		DrawUI(); // ImGUIのウィンド描画

		imGuiManager_->End();
		imGuiManager_->Draw();

		DirectXCommon::GetInstance()->PostDraw();
#else
		DirectXCommon::GetInstance()->PreDraw();
		SrvManager::GetInstance()->PreDraw();
		Draw();
		DirectXCommon::GetInstance()->PostDraw();
#endif
	}
	// ゲームの終了
	Finalize();
}