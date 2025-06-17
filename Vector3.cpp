#include "Vector3.h"
#include <math.h>

Vector3 Vector3Math::Add(const Vector3& v1, const Vector3& v2) {
	Vector3 result = {};
	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	result.z = v1.z + v2.z;
	return result;
}

Vector3 Vector3Math::Subtract(const Vector3& v1, const Vector3& v2) {
	Vector3 result = {};
	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;
	return result;
}

Vector3 Vector3Math::Multiply(float scalar, const Vector3& v) {
	Vector3 result = {};
	result.x = v.x * scalar;
	result.y = v.y * scalar;
	return result;
}

float Vector3Math::Dot(const Vector3& v1, const Vector3 v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

float Vector3Math::Length(const Vector3& v) {
	return sqrtf(Dot(v, v));
}

Vector3 Vector3Math::Normalize(const Vector3& v) {
	Vector3 result = {};
	result.x = v.x / Length(v);
	result.y = v.y / Length(v);
	result.z = v.z / Length(v);
	return result;
}