#pragma once
#include "base/ImGuiManager.h"
#include "base/RenderTarget.h"
#include "effect/EffectManager.h"

#include <memory>
#include <wrl/client.h>
#include <d3d12.h>

// ゲーム全体
class Framework {
public: // メンバ関数

	// 初期化
	virtual void Initialize();

	// 終了
	virtual void Finalize();

	// 毎フレーム更新(deltaTimeは前フレームからの実経過時間[秒])
	virtual void Update(float deltaTime);

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

protected:
	// 派生(Game)がポストエフェクトをシーンへ渡す・UIで操作するための窓口(所有と生成・破棄はFramework)
	EffectManager* GetEffectManager() const { return effectManager_.get(); }
	// 今フレームの最終画像(ポストエフェクト適用後)のSRV番号。DebugのSceneウィンドウ表示用
	uint32_t GetFinalImageSrvIndex() const { return finalImageSrvIndex_; }

private:
	bool isEndRequest_ = false;

	std::unique_ptr<RenderTarget> sceneRenderTarget_;
	std::unique_ptr<EffectManager> effectManager_;
	uint32_t finalImageSrvIndex_ = 0;

#ifdef USE_IMGUI
	std::unique_ptr<ImGuiManager> imGuiManager_;
#endif
};

