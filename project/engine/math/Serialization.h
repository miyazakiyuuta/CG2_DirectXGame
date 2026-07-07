#pragma once
#include "math/Vector3.h"
#include "math/Quaternion.h"
#include "math/Transform.h"

#include <nlohmann/json.hpp>

// 数学系構造体 <-> JSON の相互変換(nlohmann::json の ADL フック)
// to_json/from_json を型と同じスコープに置くと、
//   nlohmann::json j = transform;  (保存)
//   j.get_to(transform);           (復元)
// のように自動で変換されるようになる。
// 配列やネスト(TransformのなかのVector3等)も再帰的に解決される。

inline void to_json(nlohmann::json& j, const Vector3& v) {
	j = nlohmann::json{ {"x", v.x}, {"y", v.y}, {"z", v.z} };
}
inline void from_json(const nlohmann::json& j, Vector3& v) {
	j.at("x").get_to(v.x);
	j.at("y").get_to(v.y);
	j.at("z").get_to(v.z);
}

inline void to_json(nlohmann::json& j, const Quaternion& q) {
	j = nlohmann::json{ {"x", q.x}, {"y", q.y}, {"z", q.z}, {"w", q.w} };
}
inline void from_json(const nlohmann::json& j, Quaternion& q) {
	j.at("x").get_to(q.x);
	j.at("y").get_to(q.y);
	j.at("z").get_to(q.z);
	j.at("w").get_to(q.w);
}

inline void to_json(nlohmann::json& j, const Transform& t) {
	j = nlohmann::json{ {"scale", t.scale}, {"rotate", t.rotate}, {"translate", t.translate} };
}
inline void from_json(const nlohmann::json& j, Transform& t) {
	j.at("scale").get_to(t.scale);
	j.at("rotate").get_to(t.rotate);
	j.at("translate").get_to(t.translate);
}
