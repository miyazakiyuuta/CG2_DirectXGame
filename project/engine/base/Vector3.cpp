#include "Vector3.h"
#include <cmath>

float Vector3::Length() const {
	return sqrtf(x * x + y * y + z * z);
}

float Vector3::LengthSquared() const {
	return x * x + y * y + z * z;
}

Vector3& Vector3::Normalize() {
	float length = Length();
	if (length > 0.0f) {
		x /= length;
		y /= length;
		z /= length;
	}
	return *this;
}

Vector3 Vector3::Normalized(const Vector3& v) {
	float length = v.Length();
	if (length > 0.0f) {
		return Vector3{ v.x / length,v.y / length,v.z / length };
	}
	return Vector3{ 0.0f,0.0f,0.0f };
}

float Vector3::Dot(const Vector3& v1, const Vector3& v2) {
	return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
}

Vector3 Vector3::Cross(const Vector3& v1, const Vector3& v2) {
	Vector3 result = {
	   v1.y * v2.z - v1.z * v2.y,
	   v1.z * v2.x - v1.x * v2.z,
	   v1.x * v2.y - v1.y * v2.x
	};
	return result;
}