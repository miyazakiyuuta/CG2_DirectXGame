#pragma once
#include "scene/AbstractSceneFactory.h"
#include "base/ImGuiManager.h"
#include "base/RenderTarget.h"

#include <memory>
#include <wrl/client.h>
#include <d3d12.h>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ゲーム全体
class Framework {
public: // メンバ関数
	
	// 初期化
	virtual void Initialize();

	// 終了
	virtual void Finalize();

	// 毎フレーム更新
	virtual void Update();

	// 描画
	virtual void Draw();

	// UI描画用(ImGuiなど)
	virtual void DrawUI() {};

	// 終了チェック
	virtual bool IsEndRequest() { return isEndRequest_; }

	// 仮想デストラクタ
	virtual ~Framework();

	// 実行
	void Run();

private:
	bool isEndRequest_ = false;

protected:
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

#ifdef USE_IMGUI
	std::unique_ptr<ImGuiManager> imGuiManager_;
	std::unique_ptr<RenderTarget> sceneRenderTarget_;

	// シーンウィンドウの情報
	struct SceneViewInfo {
		ImVec2 imagePos; // ゲーム画像の左上のスクリーン座標
		ImVec2 imageSize; // ゲーム画像のサイズ
		bool isHovered; // マウスがシーンウィンドウにあるか
	} sceneViewInfo_;
	static constexpr float kGameAspect = 1280.0f / 720.0f; // ゲームのアスペクト比
#endif
};

