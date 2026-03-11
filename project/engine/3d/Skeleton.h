#pragma once
#include "3d/Model.h"
#include "3d/Animation.h"
#include "math/Transform.h"
#include "math/Matrix4x4.h"

#include <string>
#include <vector>
#include <optional>
#include <map>

struct Joint {
	QuaternionTransform transform;
	Matrix4x4 localMatrix;
	Matrix4x4 skeletonSpaceMatrix; // skeletonSpaceでの変換行列
	std::string name;
	std::vector<int32_t> children; // 子JointのIndexのリスト。いなければ空
	int32_t index; // 自身のIndex
	std::optional<int32_t> parent; // 親JointのIndex。いなければnull
};

struct Skeleton {
	int32_t root; // RootJointのIndex
	std::map<std::string, int32_t> jointMap; // Joint名とIndexとの辞書
	std::vector<Joint> joints; // 所属しているジョイント
};

Skeleton CreateSkeleton(const Model::Node& rootNode);

int32_t CreateJoint(const Model::Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints);

// スケルトンの全ジョイントの行列を更新する
void UpdateSkeleton(Skeleton& skeleton);

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime);