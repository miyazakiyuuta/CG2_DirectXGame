#include "Matrix4x4.h"
#include "math/Vector3.h"
#include "math/Quaternion.h"
#include <cmath>
#include <assert.h>

// ファイル内のみで使用する補助関数
namespace {
	float cot(float radian) {
		return 1.0f / std::tan(radian); // std::cos(radian)/std::sin::(radian);
	}
}

Matrix4x4 Matrix4x4::operator*(const Matrix4x4& other) const {
	Matrix4x4 result = {};
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			result.m[row][col] =
				m[row][0] * other.m[0][col] +
				m[row][1] * other.m[1][col] +
				m[row][2] * other.m[2][col] +
				m[row][3] * other.m[3][col];
		}
	}
	return result;
}

Matrix4x4& Matrix4x4::operator*=(const Matrix4x4& other) {
	*this = *this * other;
	return *this;
}

Matrix4x4 Matrix4x4::Inverse() {
	float aug[4][8] = {};
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			aug[row][col] = m[row][col];
		}
	}
	// 単位行列の追加
	aug[0][4] = 1.0f;
	aug[1][5] = 1.0f;
	aug[2][6] = 1.0f;
	aug[3][7] = 1.0f;

	for (int i = 0; i < 4; i++) {
		// ピボットが0の場合下の行と入れ替える
		if (aug[i][i] == 0.0f) {
			for (int j = i + 1; j < 4; j++) {
				if (aug[j][i] != 0.0f) {
					// 行を交換する
					for (int k = 0; k < 8; k++) { // 列
						float copyNum = aug[i][k]; //もともとの上の行を代入
						aug[i][k] = aug[j][k]; //上の行
						aug[j][k] = copyNum; //下の行
					}
					break;
				}
			}
		}

		// ピボットを1にする
		float pivot = aug[i][i];
		for (int k = 0; k < 8; k++) {
			aug[i][k] /= pivot;
		}

		for (int j = 0; j < 4; j++) {
			if (j != i) {
				float factor = aug[j][i];
				for (int k = 0; k < 8; k++) {
					aug[j][k] -= factor * aug[i][k];
				}
			}
		}

	}

	Matrix4x4 result = {};
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			result.m[row][col] = aug[row][col + 4];
		}
	}

	return result;
}

Matrix4x4 Matrix4x4::Transpose() {
	Matrix4x4 result = {};
	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < 4; col++) {
			result.m[row][col] = m[col][row];
		}
	}
	return result;
}

Vector3 Matrix4x4::Transform(const Vector3& vector) const {
	Vector3 result = {};
	result.x = vector.x * m[0][0] + vector.y * m[1][0] + vector.z * m[2][0] + 1.0f * m[3][0];
	result.y = vector.x * m[0][1] + vector.y * m[1][1] + vector.z * m[2][1] + 1.0f * m[3][1];
	result.z = vector.x * m[0][2] + vector.y * m[1][2] + vector.z * m[2][2] + 1.0f * m[3][2];
	float w = vector.x * m[0][3] + vector.y * m[1][3] + vector.z * m[2][3] + 1.0f * m[3][3];
	assert(w != 0.0f);
	result /= w;
	return result;
}

Matrix4x4 Matrix4x4::Identity() {
	Matrix4x4 result = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		0,0,0,1
	};
	return result;
}

Matrix4x4 Matrix4x4::Scale(const Vector3& scale) {
	Matrix4x4 result = {
		scale.x,0,0,0,
		0,scale.y,0,0,
		0,0,scale.z,0,
		0,0,0,1
	};
	return result;
}

Matrix4x4 Matrix4x4::RotateX(float radian) {
	Matrix4x4 result = {};
	result.m[0][0] = 1.0f;
	result.m[1][1] = std::cos(radian);
	result.m[1][2] = std::sin(radian);
	result.m[2][1] = -std::sin(radian);
	result.m[2][2] = std::cos(radian);
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 Matrix4x4::RotateY(float radian) {
	Matrix4x4 result = {};
	result.m[0][0] = std::cos(radian);
	result.m[0][2] = -std::sin(radian);
	result.m[1][1] = 1.0f;
	result.m[2][0] = std::sin(radian);
	result.m[2][2] = std::cos(radian);
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 Matrix4x4::RotateZ(float radian) {
	Matrix4x4 result = {};
	result.m[0][0] = std::cos(radian);
	result.m[0][1] = std::sin(radian);
	result.m[1][0] = -std::sin(radian);
	result.m[1][1] = std::cos(radian);
	result.m[2][2] = 1.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 Matrix4x4::Rotate(const Vector3& rotate) {
	return RotateX(rotate.x) * RotateY(rotate.y) * RotateZ(rotate.z);
}

Matrix4x4 Matrix4x4::Rotate(const Quaternion& q) {
	Matrix4x4 result;
	float xx = q.x * q.x;
	float yy = q.y * q.y;
	float zz = q.z * q.z;
	float xy = q.x * q.y;
	float xz = q.x * q.z;
	float yz = q.y * q.z;
	float wx = q.w * q.x;
	float wy = q.w * q.y;
	float wz = q.w * q.z;

	result.m[0][0] = 1.0f - 2.0f * (yy + zz);
	result.m[0][1] = 2.0f * (xy + wz);
	result.m[0][2] = 2.0f * (xz - wy);
	result.m[0][3] = 0.0f;

	result.m[1][0] = 2.0f * (xy - wz);
	result.m[1][1] = 1.0f - 2.0f * (xx + zz);
	result.m[1][2] = 2.0f * (yz + wx);
	result.m[1][3] = 0.0f;

	result.m[2][0] = 2.0f * (xz + wy);
	result.m[2][1] = 2.0f * (yz - wx);
	result.m[2][2] = 1.0f - 2.0f * (xx + yy);
	result.m[2][3] = 0.0f;

	result.m[3][0] = 0.0f;
	result.m[3][1] = 0.0f;
	result.m[3][2] = 0.0f;
	result.m[3][3] = 1.0f;
	return result;
}

Matrix4x4 Matrix4x4::Translate(const Vector3& translate) {
	Matrix4x4 result = {
		1,0,0,0,
		0,1,0,0,
		0,0,1,0,
		translate.x,translate.y,translate.z,1
	};
	return result;
}

Matrix4x4 Matrix4x4::Affine(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	Matrix4x4 scaleMatrix = Scale(scale);
	Matrix4x4 rotateMatrix = Rotate(rotate);
	Matrix4x4 translateMatrix = Translate(translate);
	Matrix4x4 result = scaleMatrix * rotateMatrix * translateMatrix;
	return result;
}

Matrix4x4 Matrix4x4::Affine(const Vector3& scale, const Quaternion& rotate, const Vector3& translate) {
	Matrix4x4 scaleMatrix = Scale(scale);
	Matrix4x4 rotateMatrix = Rotate(rotate);
	Matrix4x4 translateMatrix = Translate(translate);
	Matrix4x4 result = scaleMatrix * rotateMatrix * translateMatrix;
	return result;
}

Matrix4x4 Matrix4x4::PerspectiveFov(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 result = {};
	result.m[0][0] = (1.0f / aspectRatio) * cot(fovY / 2.0f);
	result.m[1][1] = cot(fovY / 2.0f);
	result.m[2][2] = farClip / (farClip - nearClip);
	result.m[2][3] = 1.0f;
	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);
	return result;
}

Matrix4x4 Matrix4x4::Orthographic(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 result = {};
	result.m[0][0] = 2.0f / (right - left);
	result.m[1][1] = 2.0f / (top - bottom);
	result.m[2][2] = 1.0f / (farClip - nearClip);
	result.m[3][0] = (left + right) / (left - right);
	result.m[3][1] = (top + bottom) / (bottom - top);
	result.m[3][2] = nearClip / (nearClip - farClip);
	result.m[3][3] = 1.0f;
	return result;
}