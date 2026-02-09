#pragma once

#include "Framework.h"

#include "SceneManager.h"

// ゲーム全体
class Game : public Framework{
public: // メンバ関数
	// 初期化
	void Initialize() override;

	// 終了
	void Finalize() override;

	// 毎フレーム更新
	void Update() override;

	// 描画
	void Draw() override;

};

