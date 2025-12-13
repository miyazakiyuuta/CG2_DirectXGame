#include "Vector3.h"
#include <math.h>

MatrixMath::Vector3 MatrixMath::Add(const Vector3& v1, const Vector3& v2) {
	Vector3 result = {};
    result.x = v1.x + v2.x;
    result.y = v1.y + v2.y;
    result.z = v1.z + v2.z;
    return result;
}

MatrixMath::Vector3 MatrixMath::Subtract(const Vector3& v1, const Vector3& v2) {
	Vector3 result = {};
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;
	return result;
}

MatrixMath::Vector3 MatrixMath::Multiply(float scalar, const Vector3& v) {
	Vector3 result = {};
	result.x = v.x * scalar;
	result.y = v.y * scalar;
	result.z = v.z * scalar;
	return result;
}

float MatrixMath::Dot(const Vector3& v1, const Vector3 v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float MatrixMath::Length(const Vector3& v) {
	return sqrtf(Dot(v, v));
}

MatrixMath::Vector3 MatrixMath::Normalize(const Vector3& v) {
	Vector3 result = {};
	result.x = v.x / Length(v);
	result.y = v.y / Length(v);
	result.z = v.z / Length(v);
	return result;
}