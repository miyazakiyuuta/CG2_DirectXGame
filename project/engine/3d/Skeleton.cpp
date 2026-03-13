#include "3d/Skeleton.h"
#include "3d/AnimationPlayer.h"

Skeleton CreateSkeleton(const Model::Node& rootNode) {
	Skeleton skeleton;
	skeleton.root = CreateJoint(rootNode, {}, skeleton.joints);

	// 名前とindexのマッピングを行いアクセスしやすくなる
	for (const Joint& joint : skeleton.joints) {
		skeleton.jointMap.emplace(joint.name, joint.index);
	}
	return skeleton;
}

int32_t CreateJoint(const Model::Node& node, const std::optional<int32_t>& parent, std::vector<Joint>& joints) {
	Joint joint;
	joint.name = node.name;
	joint.localMatrix = node.localMatrix;
	joint.skeletonSpaceMatrix = Matrix4x4::Identity();
	joint.transform = node.transform;
	joint.index = int32_t(joints.size()); // 現在登録されている数をIndexに
	joint.parent = parent;
	joints.push_back(joint); // SkeletonのJoint列に追加
	for (const Model::Node& child : node.children) {
		// 子Joiintを作成し、そのIndexを登録
		int32_t childIndex = CreateJoint(child, joint.index, joints);
		joints[joint.index].children.push_back(childIndex);
	}
	// 自身のIndexを返す
	return joint.index;
}

void UpdateSkeleton(Skeleton& skeleton) {
	for (Joint& joint : skeleton.joints) {
		joint.localMatrix = Matrix4x4::Affine(joint.transform.scale, joint.transform.rotate, joint.transform.translate);
		if (joint.parent) {
			joint.skeletonSpaceMatrix = joint.localMatrix * skeleton.joints[*joint.parent].skeletonSpaceMatrix;
		} else {
			joint.skeletonSpaceMatrix = joint.localMatrix;
		}
	}
}

void ApplyAnimation(Skeleton& skeleton, const Animation& animation, float animationTime) {
	for (Joint& joint : skeleton.joints) {
		// 対象のJointのAnimationがあれば、値の適用を行う。
		if (auto it = animation.nodeAnimations.find(joint.name); it != animation.nodeAnimations.end()) {
			const NodeAnimation& rootNodeAnimation = (*it).second;
			joint.transform.translate = AnimationPlayer::CalculateValue(rootNodeAnimation.translate, animationTime);
			joint.transform.rotate = AnimationPlayer::CalculateValue(rootNodeAnimation.rotate, animationTime);
			joint.transform.scale = AnimationPlayer::CalculateValue(rootNodeAnimation.scale, animationTime);
		}
	}
}
