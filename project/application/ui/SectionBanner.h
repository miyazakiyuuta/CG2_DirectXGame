#pragma once
#include "progress/StageProgress.h"
#include "ui/IHudElement.h"

#include <memory>

class NumberSprite;
class Sprite;

// 区間の結果表示。区間の敵すべての結末が決まった瞬間に、画面の上の方へ
// 「PERFECT」(取り逃しなし)または「SECTION CLEAR」と、倒した数「n / m」を約2秒出す。
// 区間ごとの小さな目標(全部倒す)を達成できたかを、その場で返すためのもの。
//
// 表示だけを受け持つ(ProgressGauge・Reticleと同じ立場)。いつ出すかはシーンが
// StageProgress::GetNewlyResolvedを見て決め、Showで結果を渡す。
// 文字はエンジンにフォント描画が無いので画像で持つ(数字は既存のNumberSpriteと同じnumbers.png)
class SectionBanner : public IHudElement {
public:
	void Initialize();

	/// <summary>
	/// 区間の結果を出す。表示中なら差し替えて最初から出し直す(後から確定した区間の方を見せる)
	/// </summary>
	void Show(const StageProgress::SectionResult& result);

	// すぐに消す(リトライ・エディタのStopで、前のプレイの表示を残さないため)
	void Hide() { active_ = false; }

	/// <summary>
	/// 表示時間を進め、出だしの弾む動きとフェードを反映する。Drawの前に毎フレーム呼ぶ。
	/// ヒットストップで止めないよう実dtを渡す(HUDのフェードと同じ)
	/// </summary>
	void Update(float deltaTime);

	// 呼び出し側が先にSpriteCommon::CommonDrawSetting()を済ませていること(HUDと同じ経路)
	void Draw() override;

	// 全体の透明度の倍率(0〜1)。HUDのフェードに合わせる。Updateより前に呼ぶこと
	void SetAlpha(float alpha) override { alpha_ = alpha; }

	SectionBanner();
	~SectionBanner() override;

private:
	// 見出し(PERFECT / SECTION CLEAR)
	std::unique_ptr<Sprite> perfectLabel_;
	std::unique_ptr<Sprite> clearLabel_;
	// 「n / m」。NumberSpriteは桁数固定(足りない桁を0で埋める)なので、
	// 「03 / 04」にならないよう1桁用と2桁用を両方作っておき、値に合わせて使い分ける
	std::unique_ptr<NumberSprite> defeatedOneDigit_;
	std::unique_ptr<NumberSprite> defeatedTwoDigits_;
	std::unique_ptr<NumberSprite> totalOneDigit_;
	std::unique_ptr<NumberSprite> totalTwoDigits_;
	std::unique_ptr<Sprite> slash_;
	// 画像の縦横比(幅÷高さ)。表示の高さだけを決めれば幅が決まるよう、Initializeで画像の実寸から控えておく
	// (SpriteのGetSizeは表示サイズを書き換えると変わってしまうので、毎フレームそこから求めない)
	float perfectAspect_ = 1.0f;
	float clearAspect_ = 1.0f;
	float slashAspect_ = 1.0f; // 切り出す帯(数字と同じ高さの範囲)に対する縦横比

	// 出している結果
	bool perfect_ = false;
	int defeated_ = 0;
	int total_ = 0;

	bool active_ = false;
	float timer_ = 0.0f; // Showからの経過[秒]
	float alpha_ = 1.0f; // HUDのフェードの倍率
};
