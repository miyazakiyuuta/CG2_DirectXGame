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
#include "debug/DebugRenderer.h"
#include "effect/PostProcess.h"
#include "effect/ParticleManager.h"
#include "effect/GPUParticleManager.h"
#include "effect/Monochrome.h"
#include "effect/Vignette.h"
#include "effect/BoxFilter.h"
#include "effect/GaussianFilter.h"
#include "effect/RadialBlur.h"
#include "effect/DepthBasedOutline.h"
#include "effect/LuminanceBasedOutline.h"
#include "effect/Dissolve.h"
#include "effect/Noise.h"
#include "scene/SceneManager.h"
#include "utility/Logger.h"

#include <algorithm>
#include <chrono>

void Framework::Initialize() {
	// 以降の初期化ログをファイルにも残すため最初に開く
	Logger::Initialize();

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

	ParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	GPUParticleManager::GetInstance()->Initialize(DirectXCommon::GetInstance(), SrvManager::GetInstance());

	DebugRenderer::GetInstance()->Initialize(DirectXCommon::GetInstance());

	auto dxCommon = DirectXCommon::GetInstance();

	// シーン描画先。RTV/DSVスロットは自動採番で確保する
	// 深度はウィンドウサイズの深度と共用せず専用に持つ(リサイズしてもシーンRTとサイズが一致し続ける)
	sceneRenderTarget_ = std::make_unique<RenderTarget>();
	sceneRenderTarget_->Create(dxCommon->GetDevice(), SrvManager::GetInstance(),
		dxCommon->GetRTVCPUDescriptorHandle(dxCommon->AllocateRtvIndex()), WinApp::kClientWidth, WinApp::kClientHeight);
	sceneRenderTarget_->CreateDepthBuffer(dxCommon->GetDevice(),
		dxCommon->GetDSVCPUDescriptorHandle(dxCommon->AllocateDsvIndex()));

	effectManager_ = std::make_unique<EffectManager>();
	effectManager_->Initialize(dxCommon, SrvManager::GetInstance(),
		WinApp::kClientWidth, WinApp::kClientHeight);

#pragma region エフェクト登録
	effectManager_->AddEffect(std::make_unique<RadialBlur>());
	effectManager_->AddEffect(std::make_unique<Monochrome>());
	effectManager_->AddEffect(std::make_unique<Vignette>());
	effectManager_->AddEffect(std::make_unique<BoxFilter>());
	effectManager_->AddEffect(std::make_unique<GaussianFilter>());
	effectManager_->AddEffect(std::make_unique<LuminanceBasedOutline>());
	auto outline = std::make_unique<DepthBasedOutline>();
	auto* outlinePtr = outline.get();
	effectManager_->AddEffect(std::move(outline));
	// アウトラインが読む深度はシーンRTの専用深度
	outlinePtr->SetDepthResource(sceneRenderTarget_->GetDepthResource());
	effectManager_->AddEffect(std::make_unique<Dissolve>());
	effectManager_->AddEffect(std::make_unique<Noise>());
#pragma endregion

#ifdef USE_IMGUI
	imGuiManager_ = std::make_unique<ImGuiManager>();
	imGuiManager_->Initialize(WinApp::GetInstance(), DirectXCommon::GetInstance(), SrvManager::GetInstance());
#endif

}

void Framework::Finalize() {

	// 最終シーンの終了処理(Object3d等がここで解放される)
	SceneManager::Finalize();

#ifdef USE_IMGUI
	imGuiManager_->Finalize();
	imGuiManager_.reset();
#endif

	GPUParticleManager::GetInstance()->Finalize();
	ParticleManager::GetInstance()->Finalize();
	SoundManager::GetInstance()->Finalize();
	DebugRenderer::Finalize();
	PostProcess::Finalize();
	SpriteCommon::Finalize();
	Object3dCommon::Finalize();
	ModelManager::GetInstance()->Finalize();
	TextureManager::GetInstance()->Finalize();

	// Frameworkが持つGPUリソースをDirectXCommonより先に解放する
	effectManager_.reset();
	sceneRenderTarget_.reset();

	SrvManager::Finalize();
	Input::Finalize();
	DirectXCommon::Finalize();
	WinApp::GetInstance()->Finalize();

	Logger::Finalize();
}

void Framework::Update(float deltaTime) {
	(void)deltaTime;
	if (WinApp::GetInstance()->ProcessMessage()) {
		// ゲームループを抜ける
		isEndRequest_ = true;
	}

	Input::GetInstance()->Update();
}

void Framework::Draw() {
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
		// ブレークポイントやウィンドウ操作で止まった直後に物理が吹き飛ばないよう上限を設ける
		deltaTime = (std::min)(deltaTime, 0.1f); // Windows.hのminマクロ対策の括弧

		// 毎フレーム更新
		Update(deltaTime);
		// 終了要求が来たら抜ける
		if (IsEndRequest()) {
			break;
		}

		// パーティクルの更新(シーン更新でカメラが確定した後)
		ParticleManager::GetInstance()->Update(deltaTime);
		GPUParticleManager::GetInstance()->Update(deltaTime);
		// 描画

		// リサイズ検知
		if (WinApp::IsResized()) {
			int w = WinApp::GetNewWidth();
			int h = WinApp::GetNewHeight();
			DirectXCommon::GetInstance()->ResizeSwapChain(w, h);
			WinApp::ClearResizedFlag();
		}

		SrvManager::GetInstance()->PreDraw();

		auto commandList = DirectXCommon::GetInstance()->GetCommandList();

		// スキニングの計算はシーン描画の前にまとめて行う(計算完了のバリアもここで張る)
		Object3dCommon::GetInstance()->DispatchSkinningAll();
		// GPUパーティクルの発生・更新CSも同様にシーン描画前にまとめてDispatchする
		GPUParticleManager::GetInstance()->DispatchAll();

		sceneRenderTarget_->BeginRender(commandList);
		Draw();
		// パーティクルは半透明なので不透明オブジェクトの後に描く
		ParticleManager::GetInstance()->Draw();
		GPUParticleManager::GetInstance()->Draw();
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
