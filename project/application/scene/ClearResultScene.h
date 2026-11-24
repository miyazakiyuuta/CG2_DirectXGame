#pragma once
#include "scene/ResultScene.h"
#include "progress/StageProgress.h"

#include <memory>

class Sprite;

// クリアの結果表示。女王を倒したとき(クリア演出が終わった後)にSceneFactoryが"CLEAR"で作る。
// 共通部分(スコア・選択肢・入力)はResultSceneが持ち、ここは見た目と音の違いに加えて、
// クリアにしか無いランク(駆除報告)の段を足す。
// ランクを足すために Initialize / Update / Draw を上書きし、それぞれ最初に基底の版を呼んでから自分の分を足す
// (基底の処理を置き換えるのではなく「付け足す」上書き)
class ClearResultScene : public ResultScene {
public:
	/// <summary>
	/// 結果画面に表示するランク(駆除報告)を渡す。ResultScene::SetScoreと同じく、
	/// GamePlaySceneがクリアの遷移を要求する直前に呼ぶ(ランクの決め方はStageProgressが持ち、ここは受け取って見せるだけ)。
	/// ランクはクリアにしか無いので、受け取る窓口もこのクラスに置く
	/// </summary>
	static void SetRank(StageProgress::Rank rank);

	// 定義は.cpp側(unique_ptr<Sprite>の破棄に完全型が要るため。ResultSceneと同じ理由)
	ClearResultScene();
	~ClearResultScene() override;

	void Initialize() override;
	void Update(float deltaTime) override;
	void Draw() override;

protected:
	std::string GetResultTexturePath() const override;
	Vector4 GetResultColor() const override;
	std::string GetResultSoundPath() const override;
	// ランクの段が入るぶん、全体を上へ詰める
	Layout GetLayout() const override;

private:
	// 「RANK」の見出しと文字1つ。
	// 文字はS/A/B/Cを等幅に並べた1枚の画像から切り出す(数字画像と同じやり方)
	std::unique_ptr<Sprite> rankLabelSprite_;
	std::unique_ptr<Sprite> rankSprite_;
};
