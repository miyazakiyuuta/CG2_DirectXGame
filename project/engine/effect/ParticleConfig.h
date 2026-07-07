#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"

// パーティクル描画時のブレンドモード(グループ単位で固定)
enum class BlendMode {
	Alpha,
	Add,
	None,
	Multiply,
};

// パーティクル1粒の生成・挙動パラメータ。min/maxがあるものは範囲内の乱数で決まる
struct ParticleConfig {
	Vector3 minScale = { 1.0f,1.0f,1.0f }; // 初期スケール範囲
	Vector3 maxScale = { 1.0f,1.0f,1.0f };
	Vector3 minRotate = { 0.0f,0.0f,0.0f }; // 初期回転範囲(ビルボードのため描画に反映されるのはZのみ)
	Vector3 maxRotate = { 0.0f,0.0f,0.0f };
	Vector3 minVelocity = { -1.0f,-1.0f,-1.0f }; // 初速範囲
	Vector3 maxVelocity = { 1.0f,1.0f,1.0f };
	Vector3 acceleration = { 0.0f,0.0f,0.0f }; // 加速度(重力・浮力など)
	float lifeTimeMin = 1.0f; // 寿命(秒)の範囲
	float lifeTimeMax = 1.0f;
	Vector4 startColor = { 1.0f,1.0f,1.0f,1.0f }; // 生成時の色
	Vector4 endColor = { 1.0f,1.0f,1.0f,0.0f };   // 寿命終了時の色(既定はフェードアウト)
	float endScaleRatio = 1.0f; // 寿命終了時のスケール倍率(初期スケール比)
};
