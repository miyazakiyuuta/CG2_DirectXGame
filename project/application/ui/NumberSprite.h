#pragma once
#include "math/Vector2.h"
#include "math/Vector4.h"

#include <memory>
#include <string>
#include <vector>

class Sprite;

// 数値を桁ごとのスプライトで描く。
// エンジンにフォント描画が無いため、0〜9を横一列に等幅で並べた1枚の画像から
// 桁を切り出して並べる(Sprite::SetTextureLeftTop / SetTextureSize を使う)。
//
// なぜクラスにするか: HUDのスコアと結果画面のスコアで同じ処理が要るため。
// application/ui/ に置くのは、ゲーム固有のUI部品でエンジン層の汎用機能ではないから。
//
// 桁数は固定で、足りない桁は0で埋める(スコアが増減しても表示幅が動かない)。
class NumberSprite {
public:
	// resources/ui/numbers.png の実測値。
	// この画像は数字が上下中央の一部にしか描かれておらず(全高724のうちy=228〜484)、
	// 全高を切り出すと表示サイズに対して数字が35%の高さにしか映らないため、
	// 既定でこの範囲だけを切り出す。別の画像を使うときはInitializeで上書きする
	static constexpr float kDefaultSourceTop = 228.0f;
	static constexpr float kDefaultSourceHeight = 257.0f;

	/// <summary>
	/// 桁数分のスプライトを作る。
	/// テクスチャは「0〜9が左から等幅で並んだ画像」であることが前提
	/// (1桁の幅 = テクスチャ横幅 ÷ 10)
	/// </summary>
	/// <param name="texturePath">数字画像のパス</param>
	/// <param name="digitCount">表示する桁数(足りない桁は0で埋める)</param>
	/// <param name="sourceTop">切り出す縦範囲の上端[px]</param>
	/// <param name="sourceHeight">切り出す縦範囲の高さ[px]。0以下ならテクスチャの全高を使う</param>
	void Initialize(const std::string& texturePath, int digitCount,
		float sourceTop = kDefaultSourceTop, float sourceHeight = kDefaultSourceHeight);

	// 表示する値。負値は0として扱う
	void SetValue(int value);
	// 左上を基準にした表示位置[px]
	void SetPosition(const Vector2& position);
	// 1桁あたりの表示サイズ[px]と、桁間の隙間[px]
	void SetDigitSize(const Vector2& size, float gap = 0.0f);
	void SetColor(const Vector4& color);

	// 頂点と行列の書き込み。Drawの前に毎フレーム呼ぶ
	void Update();
	void Draw();

	/// <summary>
	/// 画像が見つからず初期化できなかった場合はfalse。
	/// このときUpdate/Drawは何もしない(素材未配置でも起動できるようにするため)
	/// </summary>
	bool IsValid() const { return valid_; }

	NumberSprite();
	~NumberSprite();

private:
	// 位置・サイズが変わったので各桁のスプライトへ反映し直す
	void ApplyLayout();

	std::vector<std::unique_ptr<Sprite>> digits_;
	bool valid_ = false;

	// 数字画像1文字分の切り出しサイズ(テクスチャ実寸 ÷ 10 が横幅)と、切り出す縦範囲の上端
	Vector2 sourceDigitSize_ = { 0.0f, 0.0f };
	float sourceTop_ = 0.0f;

	Vector2 position_ = { 0.0f, 0.0f };
	Vector2 digitSize_ = { 32.0f, 48.0f };
	float gap_ = 0.0f;
};
