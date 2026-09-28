#pragma once
#include "scene/BaseScene.h"

#include <memory>

class Sprite;

// タイトル画面。作品名・開始案内・操作説明を出し、開始入力でGamePlaySceneへ進む。
// 表示と入力の作法はResultSceneに揃えている(文言は画像、中央アンカー、1280x720基準で拡大)
class TitleScene :public BaseScene {
public:

	// 定義は.cpp側(unique_ptr<Sprite>の破棄に完全型が要るため。ResultSceneと同じ理由)
	TitleScene();

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	~TitleScene() override;

private:
	// 開始入力が入ったか(Enter/Space/パッドA/START)。結果画面のリトライと同じ組み合わせにして、
	// 「決定」の操作をゲーム全体で1つに揃える
	bool IsStartPressed() const;

	// 作品名(title.png)・開始案内(start.png)・操作説明(controls.png)。
	// エンジンにフォント描画が無いため、文言はすべて画像として持つ
	std::unique_ptr<Sprite> titleSprite_;
	std::unique_ptr<Sprite> startSprite_;
	std::unique_ptr<Sprite> controlsSprite_;

	// 開始案内を明滅させるための経過時間[秒]
	float elapsed_ = 0.0f;
};
