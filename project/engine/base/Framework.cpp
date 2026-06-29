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
#include "effect/PostProcess.h"
#include "effect/Monochrome.h"
#include "effect/Vignette.h"
#include "effect/BoxFilter.h"
#include "effect/GaussianFilter.h"
#include "effect/RadialBlur.h"
#include "effect/DepthBasedOutline.h"
#include "effect/Dissolve.h"
#include "effect/Noise.h"
#include "utility/Logger.h"

#include <chrono>

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

	PostProcess::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	auto dxCommon = DirectXCommon::GetInstance();

	sceneRenderTarget_ = std::make_unique<RenderTarget>();
	sceneRenderTarget_->Create(dxCommon->GetDevice(), SrvManager::GetInstance(), 
		dxCommon->GetRTVCPUDescriptorHandle(2), WinApp::kClientWidth, WinApp::kClientHeight);
	
	effectManager_ = std::make_unique<EffectManager>();
	effectManager_->Initialize(dxCommon, SrvManager::GetInstance(),
		3, 4, WinApp::kClientWidth, WinApp::kClientHeight);

	// エフェクト登録
	effectManager_->AddEffect(std::make_unique<RadialBlur>());
	effectManager_->AddEffect(std::make_unique<Monochrome>());
	effectManager_->AddEffect(std::make_unique<Vignette>());
	effectManager_->AddEffect(std::make_unique<BoxFilter>());
	auto gaussian = std::make_unique<GaussianFilter>();
	gaussian->SetIntermediateRtvIndex(5);
	effectManager_->AddEffect(std::move(gaussian));
	auto outline = std::make_unique<DepthBasedOutline>();
	effectManager_->AddEffect(std::move(outline));
	effectManager_->AddEffect(std::make_unique<Dissolve>());
	effectManager_->AddEffect(std::make_unique<Noise>());
#ifdef USE_IMGUI
	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
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

	auto prevTime = std::chrono::steady_clock::now();

	while (true) { // ゲームループ
#ifdef USE_IMGUI
		imGuiManager_->Begin();
#endif

		auto now = std::chrono::steady_clock::now();
		float deltaTime = std::chrono::duration<float>(now - prevTime).count();
		prevTime = now;

		// 毎フレーム更新
		Update();
		// 終了要求が来たら抜ける
		if (IsEndRequest()) {
			break;
		}
		// 描画

		// リサイズ検知
		if (WinApp::IsResized()) {
			int w = WinApp::GetNewWidth();
			int h = WinApp::GetNewHeight();
			DirectXCommon::GetInstance()->ResizeSwapChain(w, h);
			WinApp::ClearResizedFlag();

			if (auto* e = effectManager_->FindEffect("DepthBasedOutline")) {
				static_cast<DepthBasedOutline*>(e)->OnResize();
			}
		}

		SrvManager::GetInstance()->PreDraw();

		auto commandList = DirectXCommon::GetInstance()->GetCommandList();
		auto dsvHandle = DirectXCommon::GetInstance()->GetDSVCPUDescriptorHandle(0);

		sceneRenderTarget_->BeginRender(commandList, dsvHandle);
		Draw();
		sceneRenderTarget_->EndRender(commandList);

		effectManager_->Update(deltaTime);

		finalImageSrvIndex_ = effectManager_->Apply(sceneRenderTarget_->GetSrvIndex());

		DirectXCommon::GetInstance()->PreDraw();

#ifdef USE_IMGUI
		DrawUI(); // ImGUIのウィンド描画
		imGuiManager_->End();
		imGuiManager_->Draw();
#else
		PostProcess::GetInstance()->Draw(finalImageSrvIndex_);
#endif

		DirectXCommon::GetInstance()->PostDraw();

	}
	// ゲームの終了
	Finalize();
}