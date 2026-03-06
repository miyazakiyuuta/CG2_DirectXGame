#pragma once
// #include "math/Vector3.h"
// #include "math/Quaternion.h"

struct Vector3;
struct Quaternion;

struct Matrix4x4 {
	float m[4][4];

	/* 演算子オーバーロード */
	Matrix4x4 operator*(const Matrix4x4& other) const;
	Matrix4x4& operator*=(const Matrix4x4& other);

	/* インスタンスメンバ関数 */
	Matrix4x4 Inverse();
	Matrix4x4 Transpose();
	Vector3 Transform(const Vector3& vector) const;

	/* 静的メンバ関数 */

	static Matrix4x4 Identity();
	static Matrix4x4 Scale(const Vector3& scale);
	
	// 回転系
	static Matrix4x4 RotateX(float radian);
	static Matrix4x4 RotateY(float radian);
	static Matrix4x4 RotateZ(float radian);
	static Matrix4x4 Rotate(const Vector3& rotate);
	static Matrix4x4 Rotate(const Quaternion& q);

	static Matrix4x4 Translate(const Vector3& translate);	
	
	// アフィン変換
	static Matrix4x4 Affine(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	static Matrix4x4 Affine(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	// 投影・ビュー
	static Matrix4x4 PerspectiveFov(float fovY, float aspectRatio, float nearClip, float farClip);
	static Matrix4x4 Orthographic(float left, float top, float right, float bottom, float nearClip, float farClip);
};

/*
namespace MatrixMath {

	Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	Matrix4x4 Inverse(const Matrix4x4& m);

	Matrix4x4 Transpose(const Matrix4x4& m);

	Matrix4x4 MakeIdentity4x4();

	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	Matrix4x4 MakeScaleMatrix(const Vector3& scale);
	
	Vector3 TransformMatrix(const Vector3& vector, const Matrix4x4& matrix);
	
	Matrix4x4 MakeRotateXMatrix(float radian);
	
	Matrix4x4 MakeRotateYMatrix(float radian);
	
	Matrix4x4 MakeRotateZMatrix(float radian);

	Matrix4x4 MakeRotateMatrix(const Vector3& rotate);
	
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	float cot(float radian); // mathFunction.hに移動

	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	Matrix4x4 MakeRotateMatrix(const Quaternion& q);

	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

}
*/