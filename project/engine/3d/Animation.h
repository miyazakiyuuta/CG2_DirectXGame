#pragma once
#include "math/Vector3.h"
#include "math/Quaternion.h"

#include <vector>
#include <map>
#include <string>

/*
struct KeyframeVector3 {
	Vector3 value; //!< キーフレームの値
	float time; //!< キーフレームの時刻(単位は秒)
};

struct KeyframeQuaternion {
	Quaternion value; //!< キーフレームの値
	float time; //!< キーフレームの時刻(単位は秒)
};
*/


template <typename tValue>
struct Keyframe {
	float time;
	tValue value;
};
using KeyframeVector3 = Keyframe<Vector3>;
using KeyframeQuaternion = Keyframe<Quaternion>;


struct NodeAnimation {
	std::vector<KeyframeVector3> translate;
	std::vector<KeyframeQuaternion> rotate;
	std::vector<KeyframeVector3> scale;
};


// Animation構造体のnodeAnimationsの型を修正
struct Animation {
	float duration; // アニメーションの全体の尺(単位は秒)
	// NodeAnimationの集合。Node名でひけるようにしておく
	std::map<std::string, NodeAnimation> nodeAnimations;
};