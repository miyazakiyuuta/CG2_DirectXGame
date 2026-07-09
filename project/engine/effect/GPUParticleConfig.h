#pragma once
#include "math/Vector3.h"
#include "math/Vector4.h"

// GPUパーティクル1粒の生成・挙動パラメータ。min/maxがあるものはCS内の乱数で範囲内に決まる。
// CPU側のParticleConfigと似ているがGPU専用(回転なし・ブレンドはAdd固定のため項目が少ない)
struct GPUParticleConfig {
	Vector3 minScale = { 1.0f,1.0f,1.0f }; // 初期スケール範囲
	Vector3 maxScale = { 1.0f,1.0f,1.0f };
	Vector3 minVelocity = { -1.0f,-1.0f,-1.0f }; // 初速範囲
	Vector3 maxVelocity = { 1.0f,1.0f,1.0f };
	Vector3 acceleration = { 0.0f,0.0f,0.0f }; // 加速度(重力・浮力など)
	float lifeTimeMin = 1.0f; // 寿命(秒)の範囲
	float lifeTimeMax = 1.0f;
	Vector4 startColor = { 1.0f,1.0f,1.0f,1.0f }; // 生成時の色
	Vector4 endColor = { 1.0f,1.0f,1.0f,0.0f };   // 寿命終了時の色(既定はフェードアウト)
	float endScaleRatio = 1.0f; // 寿命終了時のスケール倍率(初期スケール比)
};
