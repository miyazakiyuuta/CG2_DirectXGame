#pragma once
#include "Vector3.h"

struct Matrix4x4 {
	float m[4][4];
};

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
	
	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	float cot(float radian);

	Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

}