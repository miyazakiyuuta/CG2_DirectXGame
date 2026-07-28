#include "debug/SkeletonDebug.h"

#include "3d/Camera.h"
#include "debug/DebugRenderer.h"
#include "math/Vector3.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include "io/Input.h"
#endif

namespace {
	// 行列の平行移動成分＝その座標系の原点位置
	Vector3 GetTranslation(const Matrix4x4& matrix) {
		return { matrix.m[3][0], matrix.m[3][1], matrix.m[3][2] };
	}
}

void SkeletonDebug::Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix, const Options& options) {
	if (skeleton.joints.empty()) {
		return;
	}
	// 軸は選択関節のみが対象なので、選択が無ければ軸の描画は起きない
	const bool hasSelection =
		options.selectedJointIndex >= 0 &&
		options.selectedJointIndex < static_cast<int>(skeleton.joints.size());
	const bool drawsAxes = options.drawAxes && hasSelection;

	if (!options.drawBones && !options.drawJoints && !drawsAxes) {
		return;
	}

	DebugRenderer* debugRenderer = DebugRenderer::GetInstance();

	// 各関節のワールド行列を先に作る(骨の線で親を参照するため全件必要)
	std::vector<Matrix4x4> jointWorldMatrices;
	jointWorldMatrices.reserve(skeleton.joints.size());
	for (const Joint& joint : skeleton.joints) {
		jointWorldMatrices.push_back(joint.skeletonSpaceMatrix * worldMatrix);
	}

	for (size_t i = 0; i < skeleton.joints.size(); ++i) {
		const Joint& joint = skeleton.joints[i];
		const Matrix4x4& jointWorldMatrix = jointWorldMatrices[i];
		const Vector3 jointPosition = GetTranslation(jointWorldMatrix);

		// 骨: 自分と親を結ぶ線。ルートには親がいないので線は引かない
		if (options.drawBones && joint.parent) {
			debugRenderer->AddLine(
				GetTranslation(jointWorldMatrices[*joint.parent]),
				jointPosition,
				options.boneColor);
		}

		const bool isSelected = hasSelection && static_cast<int>(i) == options.selectedJointIndex;

		if (options.drawJoints) {
			// 選択中の関節は大きく白い球にして、密集した関節の中から見つけられるようにする
			if (isSelected) {
				debugRenderer->AddSphere(jointPosition, options.jointRadius * 2.0f,
					{ 1.0f, 1.0f, 1.0f, 1.0f });
			} else {
				debugRenderer->AddSphere(jointPosition, options.jointRadius, options.jointColor);
			}
		}

		// ローカル軸: 関節の姿勢そのもの。行列の各行が回転後の基底ベクトルなので
		// それを軸の長さ分だけ伸ばして描く(スケールが乗るため正規化しておく)。
		// 選択した1関節にだけ出す
		if (drawsAxes && isSelected) {
			const Vector4 axisColors[3] = {
				{ 1.0f, 0.2f, 0.2f, 1.0f }, // X
				{ 0.2f, 1.0f, 0.2f, 1.0f }, // Y
				{ 0.2f, 0.4f, 1.0f, 1.0f }, // Z
			};
			for (int axis = 0; axis < 3; ++axis) {
				Vector3 direction{
					jointWorldMatrix.m[axis][0],
					jointWorldMatrix.m[axis][1],
					jointWorldMatrix.m[axis][2]
				};
				if (direction.LengthSquared() <= 0.0f) {
					continue;
				}
				direction.Normalize();
				const Vector3 axisEnd = jointPosition + direction * options.axisLength;
				debugRenderer->AddLine(jointPosition, axisEnd, axisColors[axis]);
				// 線だけだとどちらが先端か分からないので、先端に小さな球を置く
				debugRenderer->AddSphere(axisEnd, options.jointRadius * 0.8f, axisColors[axis]);
			}
		}
	}
}

void SkeletonDebug::DrawNames(const Skeleton& skeleton, const Matrix4x4& worldMatrix,
	const Camera& camera, const Options& options) {
#ifdef USE_IMGUI
	if (skeleton.joints.empty()) {
		return;
	}

	// 選択中の関節名は、名前表示がOFFでも出す。
	// どの骨を選んでいるか分からないと軸表示の意味が無いため
	const bool hasSelection =
		options.selectedJointIndex >= 0 &&
		options.selectedJointIndex < static_cast<int>(skeleton.joints.size());
	if (!options.drawNames && !hasSelection) {
		return;
	}

	// スクリーン座標が絡むので、画面全体ではなくSceneイメージの矩形を基準にする
	const ImVec2 scenePos = Input::GetInstance()->GetSceneImagePos();
	const ImVec2 sceneSize = Input::GetInstance()->GetSceneImageSize();
	if (sceneSize.x <= 0.0f || sceneSize.y <= 0.0f) {
		return;
	}

	const Matrix4x4& viewProjectionMatrix = camera.GetViewProjectionMatrix();

	// ImGuizmoと同じ理由で、同名Beginして"Scene"ウィンドウのDrawListへ追記する
	ImGui::Begin("Scene");
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImU32 textColor = IM_COL32(255, 255, 255, 255);
	const ImU32 shadowColor = IM_COL32(0, 0, 0, 200);
	const ImU32 selectedColor = IM_COL32(255, 230, 90, 255); // 選択中は色を変えて目立たせる

	// 既に名前を描いた位置。近すぎるものを省いて文字の重なりを防ぐ
	std::vector<ImVec2> drawnPositions;
	drawnPositions.reserve(skeleton.joints.size());
	const float minDistanceSquared = options.nameMinDistance * options.nameMinDistance;

	for (size_t i = 0; i < skeleton.joints.size(); ++i) {
		const Joint& joint = skeleton.joints[i];
		const bool isSelected = hasSelection && static_cast<int>(i) == options.selectedJointIndex;

		// 名前表示OFFのときは選択中の関節だけを描く
		if (!options.drawNames && !isSelected) {
			continue;
		}

		const Matrix4x4 jointWorldMatrix = joint.skeletonSpaceMatrix * worldMatrix;
		const Vector3 position = GetTranslation(jointWorldMatrix);

		// Matrix4x4::Transformはwで割る際にw!=0をassertするので、
		// カメラ後方(w<=0)を弾くために同次座標のまま自前で変換する
		const float clipX = position.x * viewProjectionMatrix.m[0][0] + position.y * viewProjectionMatrix.m[1][0]
			+ position.z * viewProjectionMatrix.m[2][0] + viewProjectionMatrix.m[3][0];
		const float clipY = position.x * viewProjectionMatrix.m[0][1] + position.y * viewProjectionMatrix.m[1][1]
			+ position.z * viewProjectionMatrix.m[2][1] + viewProjectionMatrix.m[3][1];
		const float clipW = position.x * viewProjectionMatrix.m[0][3] + position.y * viewProjectionMatrix.m[1][3]
			+ position.z * viewProjectionMatrix.m[2][3] + viewProjectionMatrix.m[3][3];
		if (clipW <= 0.0f) {
			continue; // カメラの後ろにある関節は描かない
		}

		// NDC(-1〜1) → Sceneイメージ内のスクリーン座標。Yは上下が逆
		const float ndcX = clipX / clipW;
		const float ndcY = clipY / clipW;
		if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) {
			continue; // 画面外は描かない(文字が縁に貼り付くのを防ぐ)
		}
		const ImVec2 screenPosition{
			scenePos.x + (ndcX * 0.5f + 0.5f) * sceneSize.x,
			scenePos.y + (0.5f - ndcY * 0.5f) * sceneSize.y
		};

		// 先に描いた名前と近すぎるものは省く(指の関節が密集して読めなくなるのを防ぐ)。
		// 選択中の関節は間引きの対象外にして、必ず見えるようにする
		if (!isSelected && minDistanceSquared > 0.0f) {
			bool tooClose = false;
			for (const ImVec2& drawn : drawnPositions) {
				const float dx = drawn.x - screenPosition.x;
				const float dy = drawn.y - screenPosition.y;
				if (dx * dx + dy * dy < minDistanceSquared) {
					tooClose = true;
					break;
				}
			}
			if (tooClose) {
				continue;
			}
		}
		drawnPositions.push_back(screenPosition);

		// 背景がモデル色に埋もれるので、1pxずらした影を先に描いて可読性を確保する
		drawList->AddText({ screenPosition.x + 1.0f, screenPosition.y + 1.0f }, shadowColor, joint.name.c_str());
		drawList->AddText(screenPosition, isSelected ? selectedColor : textColor, joint.name.c_str());
	}

	ImGui::End();
#else
	// Debug構成以外では文字描画の手段がないので何もしない
	(void)skeleton; (void)worldMatrix; (void)camera; (void)options;
#endif
}

void SkeletonDebug::DrawImGui(const Skeleton& skeleton, Options& options) {
#ifdef USE_IMGUI
	ImGui::Checkbox("Bones", &options.drawBones);
	ImGui::SameLine();
	ImGui::Checkbox("Joints", &options.drawJoints);
	ImGui::SameLine();
	ImGui::Checkbox("Names", &options.drawNames);

	ImGui::DragFloat("Joint Radius", &options.jointRadius, 0.002f, 0.001f, 0.5f);
	ImGui::DragFloat("Name Spacing", &options.nameMinDistance, 0.5f, 0.0f, 200.0f);
	ImGui::ColorEdit4("Bone Color", &options.boneColor.x);
	ImGui::ColorEdit4("Joint Color", &options.jointColor.x);

	// --- 選択した関節のローカル軸 ---------------------------------------
	// 「どの骨がどっちを向いているか」を調べるための機能なので、対象を1つ選ばせる。
	// 全関節に出すと線が重なって読めず、実用にならない
	ImGui::SeparatorText("Selected Joint");

	const bool hasSelection =
		options.selectedJointIndex >= 0 &&
		options.selectedJointIndex < static_cast<int>(skeleton.joints.size());
	const char* preview = hasSelection
		? skeleton.joints[options.selectedJointIndex].name.c_str()
		: "(none)";

	if (ImGui::BeginCombo("Joint", preview)) {
		if (ImGui::Selectable("(none)", !hasSelection)) {
			options.selectedJointIndex = -1;
		}
		for (int i = 0; i < static_cast<int>(skeleton.joints.size()); ++i) {
			// 同名ジョイントがあってもIDが衝突しないようindexをIDに使う
			ImGui::PushID(i);
			if (ImGui::Selectable(skeleton.joints[i].name.c_str(), options.selectedJointIndex == i)) {
				options.selectedJointIndex = i;
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}

	ImGui::BeginDisabled(!hasSelection);
	ImGui::Checkbox("Show Local Axis (X=R / Y=G / Z=B)", &options.drawAxes);
	ImGui::DragFloat("Axis Length", &options.axisLength, 0.005f, 0.01f, 2.0f);
	ImGui::EndDisabled();

	if (!hasSelection) {
		ImGui::TextDisabled("Select a joint to show its local axis.");
	}
#else
	(void)skeleton; (void)options;
#endif
}
