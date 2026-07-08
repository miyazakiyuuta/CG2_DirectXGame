#include "TransformGizmo.h"

#ifdef USE_IMGUI
#include "3d/Camera.h"
#include "base/WinApp.h"
#include "io/Input.h"
#include "math/Matrix4x4.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <imgui.h>
#include <ImGuizmo.h>

namespace {
	// 現在の操作モード(ギズモは選択中の1体にのみ出すため全体で共有)
	ImGuizmo::OPERATION currentOperation = ImGuizmo::TRANSLATE;
}

void TransformGizmo::DrawOperationSelector() {
	if (ImGui::RadioButton("Translate", currentOperation == ImGuizmo::TRANSLATE)) { currentOperation = ImGuizmo::TRANSLATE; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", currentOperation == ImGuizmo::ROTATE)) { currentOperation = ImGuizmo::ROTATE; }
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", currentOperation == ImGuizmo::SCALE)) { currentOperation = ImGuizmo::SCALE; }
}

void TransformGizmo::PickBySceneClick(const Camera& camera, const std::vector<EditorObject>& objects, int& selectedIndex) {
	// Sceneビュー上で左ボタンを押した瞬間のみ判定
	if (!Input::GetInstance()->IsSceneViewHovered() || !ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return;
	}
	// ギズモを掴む/操作中のクリックは選択変更しない(ギズモ表示中のみ判定が有効)
	if (selectedIndex >= 0 && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) {
		return;
	}
	float mouseX = 0.0f;
	float mouseY = 0.0f;
	if (!Input::GetInstance()->GetSceneMousePos(mouseX, mouseY)) {
		return;
	}

	// スクリーン座標 → NDC(-1〜+1)。除数はGetSceneMousePosが返す仮想解像度と対
	float ndcX = (mouseX / static_cast<float>(WinApp::kClientWidth)) * 2.0f - 1.0f;
	float ndcY = 1.0f - (mouseY / static_cast<float>(WinApp::kClientHeight)) * 2.0f;

	// near平面(z=0)とfar平面(z=1)の点をviewProj逆行列でワールドへ戻し、カメラ発のレイを作る
	Matrix4x4 invViewProj = camera.GetViewProjectionMatrix().Inverse();
	Vector3 rayOrigin = invViewProj.Transform({ ndcX, ndcY, 0.0f });
	Vector3 rayFar = invViewProj.Transform({ ndcX, ndcY, 1.0f });
	Vector3 rayDir = Vector3::Normalized(rayFar - rayOrigin);

	// 全対象とレイ-球の交差判定を行い、最も手前のヒットを選択。ヒットなしなら選択解除
	int nearestIndex = -1;
	float nearestT = (std::numeric_limits<float>::max)(); // Windows.hのmaxマクロ対策の括弧
	for (int i = 0; i < static_cast<int>(objects.size()); ++i) {
		const EditorObject& target = objects[i];
		if (!target.pickable || !target.transform) {
			continue;
		}
		float radius = target.pickRadius;
		if (radius <= 0.0f) {
			// モデルが単位サイズ前提でscaleの最大成分を半径とする
			const Vector3& s = target.transform->scale;
			radius = (std::max)({ s.x, s.y, s.z });
		}

		Vector3 toCenter = target.transform->translate - rayOrigin;
		float tClosest = Vector3::Dot(toCenter, rayDir); // レイ上で球中心に最も近い点までの距離
		if (tClosest < 0.0f) {
			continue; // 球がカメラの後ろ
		}
		float distSq = toCenter.LengthSquared() - tClosest * tClosest; // 球中心とレイの距離^2
		if (distSq > radius * radius) {
			continue; // レイが球を通らない
		}
		float tHit = tClosest - std::sqrt(radius * radius - distSq); // 球表面への入射点
		if (tHit < nearestT) {
			nearestT = tHit;
			nearestIndex = i;
		}
	}
	selectedIndex = nearestIndex;
}

bool TransformGizmo::Manipulate(const Camera& camera, EditorObject& object) {
	if (!object.transform) {
		return false;
	}
	Transform& transform = *object.transform;

	// シーンは"Scene"ウィンドウ内のイメージに描画されるため、基準矩形はそのイメージの
	// スクリーン座標矩形(Game::DrawUIがInputへ渡している値)を使う
	const ImVec2& scenePos = Input::GetInstance()->GetSceneImagePos();
	const ImVec2& sceneSize = Input::GetInstance()->GetSceneImageSize();
	if (sceneSize.x <= 0.0f || sceneSize.y <= 0.0f) {
		return false;
	}

	// scale編集を許可しない対象(カメラ等)ではScaleモードをTranslateに読み替える
	ImGuizmo::OPERATION operation = currentOperation;
	if (operation == ImGuizmo::SCALE && !object.scaleEditable) {
		operation = ImGuizmo::TRANSLATE;
	}

	// ImGuizmoは「描画先DrawListを所有するウィンドウがホバーされているか」でマウス判定を
	// 行うため、同名Beginで"Scene"ウィンドウに追記してそのDrawListに描く
	ImGui::Begin("Scene");
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetOrthographic(false);
	ImGuizmo::SetRect(scenePos.x, scenePos.y, sceneSize.x, sceneSize.y);

	Matrix4x4 world = Matrix4x4::Affine(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 view = camera.GetViewMatrix();
	Matrix4x4 proj = camera.GetProjectionMatrix();

	bool changed = ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], operation, ImGuizmo::WORLD, &world.m[0][0]);
	if (changed) {
		float t[3], r[3], s[3];
		ImGuizmo::DecomposeMatrixToComponents(&world.m[0][0], t, r, s);
		transform.translate = { t[0], t[1], t[2] };
		// DecomposeMatrixToComponents の回転は度数法なのでラジアンへ変換
		constexpr float deg2Rad = std::numbers::pi_v<float> / 180.0f;
		transform.rotate = { r[0] * deg2Rad, r[1] * deg2Rad, r[2] * deg2Rad };
		transform.scale = { s[0], s[1], s[2] };
	}
	ImGui::End();

	return changed;
}
#endif
