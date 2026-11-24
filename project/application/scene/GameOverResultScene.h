#pragma once
#include "scene/ResultScene.h"

// ゲームオーバーの結果表示。HPが尽きたとき(ゲームオーバー演出が真っ黒になった後)にSceneFactoryが"GAMEOVER"で作る。
// 共通部分(スコア・選択肢・入力)はResultSceneが持ち、ここは見た目と音の違いだけを決める
class GameOverResultScene : public ResultScene {
protected:
	std::string GetResultTexturePath() const override;
	// 赤みを足して、クリアと一目で区別できるようにする
	Vector4 GetResultColor() const override;
	std::string GetResultSoundPath() const override;
	// ランクを出さないので、文言・スコア・選択肢を画面の中ほどにまとめる
	Layout GetLayout() const override;
};
