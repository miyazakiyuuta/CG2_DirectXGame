#pragma once
struct Vector3;
struct Quaternion;

struct Matrix4x4 {
	float m[4][4];

	/* 演算子オーバーロード */
	Matrix4x4 operator*(const Matrix4x4& other) const;
	Matrix4x4& operator*=(const Matrix4x4& other);

	/* インスタンスメンバ関数 */
	Matrix4x4 Inverse() const;
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
	// 回転行列 → オイラー角。Rotate(const Vector3&)の逆で、返した角度をRotateに渡すと元の行列に戻る。
	// 回転を行列の掛け算で合成した結果を、オイラー角しか受け取らないObject3d::SetRotateへ渡すときに使う
	static Vector3 ToEuler(const Matrix4x4& rotation);

	static Matrix4x4 Translate(const Vector3& translate);
	
	// アフィン変換
	static Matrix4x4 Affine(const Vector3& scale, const Vector3& rotate, const Vector3& translate);
	static Matrix4x4 Affine(const Vector3& scale, const Quaternion& rotate, const Vector3& translate);

	// 投影・ビュー
	static Matrix4x4 PerspectiveFov(float fovY, float aspectRatio, float nearClip, float farClip);
	static Matrix4x4 Orthographic(float left, float top, float right, float bottom, float nearClip, float farClip);
};