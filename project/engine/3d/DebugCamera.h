#pragma once
#include "../base/Matrix4x4.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

struct Vector3 {
	float x;
	float y;
	float z;
};

/// <summary>
/// デバッグカメラ
/// </summary>
class DebugCamera {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	void Initialize();

	/// <summary>
	/// 更新
	/// </summary>
	void Update(IDirectInputDevice8* keyboard, POINT mouseDelta, float wheelDelta);

	Matrix4x4 GetViewMatrix() { return viewMatrix_; }

private:
	// X,Y,Z軸回りのローカル回転角
	//Vector3 rotation_ = {};
	Vector3 pivot_;
	// 累積回転行列
	Matrix4x4 matRot_;
	// ローカル座標
	Vector3 translation_;
	// ビュー行列
	Matrix4x4 viewMatrix_;
	// 射影行列
	Matrix4x4 projectionMatrix_;
};

