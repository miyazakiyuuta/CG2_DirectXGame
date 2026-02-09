#pragma once

#include "AbstractSceneFactory.h"

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

	// 終了チェック
	virtual bool IsEndRequest() { return isEndRequest_; }

	// 仮想デストラクタ
	virtual ~Framework() = default;

	// 実行
	void Run();

private:
	bool isEndRequest_ = false;

protected:
	AbstractSceneFactory* sceneFactory_ = nullptr;
};

