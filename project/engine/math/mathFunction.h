#pragma once
#include "math/Vector3.h"
#include "math/Quaternion.h"
#include <cmath>

// Vector3用の線形補間
inline Vector3 Lerp(const Vector3& v1, const Vector3& v2, float t) {
	return {
		v1.x + t * (v2.x - v1.x),
		v1.y + t * (v2.y - v1.y),
		v1.z + t * (v2.z - v1.z)
	};
}

// Catmull-Romスプライン補間（4制御点で p1→p2 間を補間する）
// t は区間内の割合(0〜1)。p0/p3 は両隣の制御点で、曲線の傾き（接線）を決めるために使う
inline Vector3 CatmullRomInterpolation(const Vector3& p0, const Vector3& p1, const Vector3& p2, const Vector3& p3, float t) {
	float t2 = t * t;  // t^2
	float t3 = t2 * t; // t^3

	// 標準形: 0.5 * (2p1 + (-p0+p2)t + (2p0-5p1+4p2-p3)t^2 + (-p0+3p1-3p2+p3)t^3)
	Vector3 e3 = -p0 + p1 * 3.0f - p2 * 3.0f + p3; // t^3 の係数
	Vector3 e2 = p0 * 2.0f - p1 * 5.0f + p2 * 4.0f - p3; // t^2 の係数
	Vector3 e1 = -p0 + p2; // t の係数
	Vector3 e0 = p1 * 2.0f; // 定数項

	return (e3 * t3 + e2 * t2 + e1 * t + e0) * 0.5f;
}

// Quaternion用の球面線形補間
inline Quaternion Slerp(const Quaternion& q1, const Quaternion& q2, float t) {
    // 内積を求める（2つの回転の角度差を判定するため）
    float dot = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

    // 最短経路を通るための処理
    // 内積が負の場合、逆回転の方が近道なので片方の符号を反転させる
    Quaternion targetQ2 = q2;
    if (dot < 0.0f) {
        dot = -dot;
        targetQ2 = { -q2.x, -q2.y, -q2.z, -q2.w };
    }

    // なす角が非常に小さい場合は、誤差を避けるために通常の Lerp に切り替える
    if (dot >= 1.0f - 0.0005f) {
        return {
            q1.x + t * (targetQ2.x - q1.x),
            q1.y + t * (targetQ2.y - q1.y),
            q1.z + t * (targetQ2.z - q1.z),
            q1.w + t * (targetQ2.w - q1.w)
        };
    }

    // Slerp の本計算
    float theta = std::acos(dot); // なす角
    float sinTheta = std::sin(theta);
    float invSinTheta = 1.0f / sinTheta;

    float t1 = std::sin((1.0f - t) * theta) * invSinTheta;
    float t2 = std::sin(t * theta) * invSinTheta;

    return {
        q1.x * t1 + targetQ2.x * t2,
        q1.y * t1 + targetQ2.y * t2,
        q1.z * t1 + targetQ2.z * t2,
        q1.w * t1 + targetQ2.w * t2
    };
}