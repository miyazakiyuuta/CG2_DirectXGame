#pragma once

struct Vector3 {
	float x;
	float y;
	float z;

	Vector3(float x = 0.0f, float y = 0.0f, float z = 0.0f)
		: x(x), y(y), z(z) {
	}

	// 長さ(ノルム)
	float Length() const;
	// 長さの二乗(sqrtfは重い)
	float LengthSquared() const;

	// 正規化(自身の値を変える)
	Vector3& Normalize();
	// 正規化(戻り値)
	static Vector3 Normalized(const Vector3& v);

	// 内積
	static float Dot(const Vector3& v1, const Vector3& v2);

	// クロス積
	static Vector3 Cross(const Vector3& v1, const Vector3& v2);

	/* 演算子オーバーロード */

	Vector3 operator-()const {
		return Vector3(-x, -y, -z);
	}

	Vector3& operator+=(const Vector3& other) {
		x += other.x;
		y += other.y;
		z += other.z;
		return *this;
	}
	Vector3& operator-=(const Vector3& other) {
		x -= other.x;
		y -= other.y;
		z -= other.z;
		return *this;
	}
	Vector3& operator*=(float scalar) {
		x *= scalar;
		y *= scalar;
		z *= scalar;
		return *this;
	}
	Vector3& operator/=(float scalar) {
		if (scalar != 0.0f) {
			x /= scalar;
			y /= scalar;
			z /= scalar;
		}
		return *this;
	}

	Vector3 operator+(const Vector3& other) const {
		Vector3 result = *this;
		result += other;
		return result;
	}
	Vector3 operator-(const Vector3& other) const {
		Vector3 result = *this;
		result -= other;
		return result;
	}
	Vector3 operator*(float scalar) const {
		Vector3 result = *this;
		result *= scalar;
		return result;
	}
	Vector3 operator/(float scalar)const {
		Vector3 result = *this;
		result /= scalar;
		return result;
	}
};