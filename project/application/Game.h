#pragma once
#include "base/Framework.h"
#include "scene/AbstractSceneFactory.h"

#include <memory>

#ifdef USE_IMGUI
#include <imgui.h>
#endif

// ゲーム全体
class Game : public Framework{
public: // メンバ関数
	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 毎フレーム更新
	void Update(float deltaTime) override;

	// 描画
	void Draw() override;

	void DrawUI() override;

	~Game() override;

private: // メンバ変数
	// このゲームのシーン生成役。SceneManagerは生ポインタで借りるだけなので、所有はここ
	// (SceneManagerはFinalizeで先に破棄されるため、Gameのメンバとして後から消えても安全)
	std::unique_ptr<AbstractSceneFactory> sceneFactory_;

#ifdef USE_IMGUI
	// シーンウィンドウの情報
	struct SceneViewInfo {
		ImVec2 imagePos; // ゲーム画像の左上のスクリーン座標
		ImVec2 imageSize; // ゲーム画像のサイズ
		bool isHovered; // マウスがシーンウィンドウにあるか
	} sceneViewInfo_;
	static constexpr float kGameAspect = 1280.0f / 720.0f; // ゲームのアスペクト比
#endif
};

