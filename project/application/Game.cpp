#include "Game.h"
#include "base/SrvManager.h"
#include "base/RenderTarget.h"
#include "utility/Logger.h"
#include "scene/BaseScene.h"
#include "scene/SceneFactory.h"
#include "scene/SceneManager.h"
#include "io/Input.h"
#include "effect/ParticleManager.h"
#include "effect/GPUParticleManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#endif

void Game::Initialize() {
	Framework::Initialize();

	sceneFactory_ = std::make_unique<SceneFactory>();
	SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
    SceneManager::GetInstance()->SetEffectManager(effectManager_.get());
	//SceneManager::GetInstance()->ChangeScene("TITLE");
	//SceneManager::GetInstance()->ChangeScene("GAMEPLAY");
	// CG5評価課題1: ポストエフェクトのデモシーンから起動
	SceneManager::GetInstance()->ChangeScene("POSTEFFECTDEMO");

	// 出力ウィンドウへの文字出力
	OutputDebugStringA("Hello,DirectX!\n");
}

void Game::Finalize() {

	Framework::Finalize();
}

void Game::Update(float deltaTime) {
	Framework::Update(deltaTime);
	SceneManager::GetInstance()->Update(deltaTime);
}

void Game::Draw() {
	Framework::Draw();
	SceneManager::GetInstance()->Draw();
}

void Game::DrawUI() {
#ifdef USE_IMGUI

	// 画面全体をドッキング可能な領域にする（Unityのレイアウト機能）
    ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), dockspace_flags);
#pragma region シーン描画用のウィンドウ作成
    ImGui::Begin("Scene");
    ImVec2 contentSize = ImGui::GetContentRegionAvail(); // ウィンドウの空き領域サイズを取得

    // ウィンドウに収まる最大サイズをアスペクト比を保って計算
    ImVec2 imageSize;
    float windowAspect = contentSize.x / contentSize.y;

    if (windowAspect > kGameAspect) { // ウィンドウが横長 → 高さに合わせる
        imageSize.y = contentSize.y;
        imageSize.x = contentSize.y * kGameAspect;
    } else { // ウィンドウが縦長 → 幅に合わせる
        imageSize.x = contentSize.x;
        imageSize.y = contentSize.x / kGameAspect;
    }

    // 中央に配置
    ImVec2 padding;
    padding.x = (contentSize.x - imageSize.x) * 0.5f;
    padding.y = (contentSize.y - imageSize.y) * 0.5f;
    ImGui::SetCursorPos({
        ImGui::GetCursorPos().x + padding.x,
        ImGui::GetCursorPos().y + padding.y
    });

    // 画像の左上スクリーン座標を記録
    sceneViewInfo_.imagePos = ImGui::GetCursorScreenPos();
    sceneViewInfo_.imageSize = imageSize;

    auto srvHandle = SrvManager::GetInstance()->GetGPUDescriptorHandle(finalImageSrvIndex_); // レンダーテクスチャのSRVハンドルを取得
    ImGui::Image((ImTextureID)srvHandle.ptr, imageSize); // レンダーテクスチャをウィンドウ内に画像として表示
    // マウスがSceneウィンドウ上にあるか記録
    sceneViewInfo_.isHovered = ImGui::IsWindowHovered();

    Input::GetInstance()->SetSceneViewInfo(sceneViewInfo_.imagePos, sceneViewInfo_.imageSize, sceneViewInfo_.isHovered);

	ImGui::End();
#pragma endregion

    ImGui::Begin("PostEffect");
    effectManager_->DrawImGui();
    ImGui::End();

    ImGui::Begin("Particle");
    ParticleManager::GetInstance()->DrawImGui();
    GPUParticleManager::GetInstance()->DrawImGui();
    ImGui::End();

	SceneManager::GetInstance()->DrawImGui(); // シーン固有UIの描画
#endif
}

Game::~Game() = default;
