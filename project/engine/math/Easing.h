#pragma once
#include <algorithm>
#include <cmath>
#include <numbers>

// イージング関数(動きの加減速カーブ)。
// 引数tは「経過時間 ÷ 全体の長さ」の進行率で、0〜1の外は端へ丸めてから計算する
// (呼び出し側が丸め忘れても、開始前や終了後に値が暴れないようにするため)。
// 戻り値は0から1へ向かう補間係数で、Lerp(開始値, 終了値, 係数) のように使う
namespace Easing {

	// ゆっくり動き出し、加速したまま終わる(止まらずに抜けていく)。画面の外へ去る動き向き
	inline float InCubic(float t) {
		t = std::clamp(t, 0.0f, 1.0f);
		return t * t * t;
	}

	// 勢いよく動き出し、だんだん減速して止まる。目標地点への到着向き
	inline float OutCubic(float t) {
		t = std::clamp(t, 0.0f, 1.0f);
		const float rest = 1.0f - t;
		return 1.0f - rest * rest * rest;
	}

	// ゆっくり動き出し、中盤で最も速くなり、ゆっくり止まる。InOutCubicより加減速が穏やかで、
	// 重さのある物(機体など)が速さ0から自然に動き出す様子に向く
	inline float InOutSine(float t) {
		t = std::clamp(t, 0.0f, 1.0f);
		return 0.5f - 0.5f * std::cos(std::numbers::pi_v<float> * t);
	}

	// ゆっくり動き出し、中盤で最も速くなり、ゆっくり止まる。位置の戻しや移動し直し向き
	inline float InOutCubic(float t) {
		t = std::clamp(t, 0.0f, 1.0f);
		if (t < 0.5f) {
			return 4.0f * t * t * t;
		}
		const float rest = -2.0f * t + 2.0f;
		return 1.0f - rest * rest * rest * 0.5f;
	}

	// 目標を少し行き過ぎてから戻って止まる(途中で1を超える)。登場時の「着地感」を出したいとき向き。
	// overshootが大きいほど大きく行き過ぎる(既定値1.70158で約10%)
	inline float OutBack(float t, float overshoot = 1.70158f) {
		t = std::clamp(t, 0.0f, 1.0f);
		const float rest = t - 1.0f;
		return 1.0f + (overshoot + 1.0f) * rest * rest * rest + overshoot * rest * rest;
	}
}
