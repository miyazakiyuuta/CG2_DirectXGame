#pragma once
#include "progress/StageProgress.h"

#include <memory>
#include <vector>

class Sprite;

// 進行ゲージ(画面上部の中央)。スタートから終点(女王)までのどこにいるかと、区間の区切り、
// 取り逃しなしで終えた区間(PERFECT)を1本の棒で見せる。
// 「今の区間の目標(全部倒す)」と「最後の目標(女王)」を同じ棒の上に並べ、道中の成績表も兼ねる。
//
// 表示だけを受け持つ(Reticleと同じ立場)。数え方はStageProgress、進行度はシーンが持っていて、
// ここは受け取った結果を並べるだけ。
// 座標の基準はシーンRTのピクセル座標で、1280x720基準のレイアウトに実解像度の倍率を掛ける(HUDと同じ)
class ProgressGauge {
public:
	void Initialize();

	/// <summary>
	/// 各パーツの位置と色を決める。Drawの前に毎フレーム呼ぶ
	/// </summary>
	/// <param name="sections">区間ごとの集計(StageProgress::GetSections)。区切りの位置とPERFECTの表示に使う</param>
	/// <param name="railDistance">今の進行度[m]</param>
	/// <param name="railLength">レールの全長[m](ゲージの右端)</param>
	/// <param name="hasGoal">終点の先に女王がいるか。いれば右端の印を女王の色にする</param>
	void Update(const std::vector<StageProgress::SectionResult>& sections, float railDistance, float railLength,
		bool hasGoal);

	// 呼び出し側が先にSpriteCommon::CommonDrawSetting()を済ませていること(HUDと同じ経路)
	void Draw();

	// 全体の透明度の倍率(0〜1)。HUDのフェードに合わせる。Updateより前に呼ぶこと
	void SetAlpha(float alpha) { alpha_ = alpha; }

	ProgressGauge();
	~ProgressGauge();

private:
	// 区間の数に合わせて、区間ごとのパーツ(PERFECTの帯・区切りの目盛り)を用意する。
	// 増やすだけで減らさない(余りは描かない): エディタで区間を消しても、作ったスプライトを壊さずに済む
	void EnsureSectionParts(size_t count);

	// 帯(左端基準)。土台の縁取り・溝・進んだ分
	std::unique_ptr<Sprite> outline_;
	std::unique_ptr<Sprite> track_;
	std::unique_ptr<Sprite> fill_;
	// 区間ごとのPERFECTの帯と、区切りの目盛り(縁取り＋本体)
	std::vector<std::unique_ptr<Sprite>> perfectBands_;
	std::vector<std::unique_ptr<Sprite>> tickOutlines_;
	std::vector<std::unique_ptr<Sprite>> ticks_;
	// 終点(女王)の印と、自機の今の位置の印(縁取り＋本体)
	std::unique_ptr<Sprite> goalOutline_;
	std::unique_ptr<Sprite> goal_;
	std::unique_ptr<Sprite> markerOutline_;
	std::unique_ptr<Sprite> marker_;

	// 今フレーム使っている区間の数(perfectBands_などの先頭からこの数だけ描く)
	size_t sectionCount_ = 0;
	float alpha_ = 1.0f;
};
