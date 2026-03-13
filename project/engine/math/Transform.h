#pragma once
#include "math/Vector3.h"
#include "math/Quaternion.h"

struct Transform {
	Vector3 scale = {1.0f, 1.0f, 1.0f};
	Vector3 rotate = {0.0f, 0.0f, 0.0f};
	Vector3 translate = {0.0f, 0.0f, 0.0f};
};

// エル(オイラー角)ベース
struct EulerTransform {
	Vector3 scale;
	Vector3 rotate; // Eulerでの回転
	Vector3 translate;
};

// Quternionベース
struct QuaternionTransform {
	Vector3 scale;
	Quaternion rotate;
	Vector3 translate;
};