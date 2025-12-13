#pragma once

namespace MatrixMath {
	struct Vector3 {
		float x;
		float y;
		float z;
		// 追加: const参照への暗黙変換演算子
		operator const Vector3& () const { return *this; }
	};

	struct Vector2 {
		float x;
		float y;
	};

	struct Vector4 {
		float x;
		float y;
		float z;
		float w;
	};

	struct Transform {
		Vector3 scale;
		Vector3 rotate;
		Vector3 translate;
	};

	// 加算
	Vector3 Add(const Vector3& v1, const Vector3& v2);
	// 減算
	Vector3 Subtract(const Vector3& v1, const Vector3& v2);
	// スカラー倍
	Vector3 Multiply(float scalar, const Vector3& v);
	// 内積
	float Dot(const Vector3& v1, const Vector3 v2);
	// 長さ(ノルム)
	float Length(const Vector3& v);
	// 正規化
	Vector3 Normalize(const Vector3& v);

}
