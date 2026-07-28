#pragma once
#include "3d/Skeleton.h"
#include "math/Matrix4x4.h"
#include "math/Vector4.h"

class Camera;

/// <summary>
/// スケルトン(骨)のデバッグ可視化。
/// 骨の線・関節の球・ローカル軸は DebugRenderer 経由なので全構成で動作する。
/// 関節名だけは ImGui の文字描画が必要なため Debug(USE_IMGUI) 構成のみ有効。
///
/// Object3d に持たせず独立させているのは、可視化の設定(何を出すか・色・サイズ)が
/// 描画対象ではなくデバッグ用途ごとに変わるため。呼び出し側がOptionsを所有する
/// </summary>
namespace SkeletonDebug {

	struct Options {
		bool drawBones = true;   // 親子の関節を結ぶ線(骨そのもの)
		bool drawJoints = true;  // 関節位置の球
		bool drawNames = false;  // 関節名(Debug構成のみ)

		// ローカル軸(X=赤 / Y=緑 / Z=青)は「選んだ1関節だけ」に出す。
		// 人型リグは指だけで40関節ほどあり、全関節に出すと線が重なって読めないため。
		// 実用上も知りたいのは特定の骨の向き(武器を持たせる手など)に限られる
		bool drawAxes = false;
		int selectedJointIndex = -1; // 軸と強調表示の対象。-1なら未選択

		float jointRadius = 0.02f; // 関節の球の半径[m]
		float axisLength = 0.25f;  // ローカル軸の長さ[m]。1関節だけなので長めでよい

		// 名前を描く最小間隔[px]。人型リグは指だけで40関節ほどあり、全部描くと
		// 文字が重なって読めなくなるため、既に描いた名前の近くにあるものは省く。
		// 0にすると全関節の名前を描く
		float nameMinDistance = 24.0f;

		Vector4 boneColor{ 0.2f, 1.0f, 0.4f, 1.0f };
		Vector4 jointColor{ 1.0f, 0.85f, 0.2f, 1.0f };
	};

	/// <summary>
	/// 骨・関節・ローカル軸を DebugRenderer へ積む(実描画は DebugRenderer::RenderAll)。
	/// スケルトンのポーズは Object3d::Update で更新済みである必要がある
	/// </summary>
	/// <param name="skeleton">対象スケルトン(Object3d::GetSkeleton())</param>
	/// <param name="worldMatrix">スケルトン空間→ワールド空間の変換(Object3d::GetWorldMatrix())</param>
	void Draw(const Skeleton& skeleton, const Matrix4x4& worldMatrix, const Options& options);

	/// <summary>
	/// 関節名を "Scene" ウィンドウのDrawListへ描く。
	/// ImGuiのフレーム内(シーンのDrawImGui)から呼ぶこと。Debug構成以外では何もしない
	/// </summary>
	void DrawNames(const Skeleton& skeleton, const Matrix4x4& worldMatrix,
		const Camera& camera, const Options& options);

	/// <summary>
	/// Options を編集するImGui。呼び出し側が開いているウィンドウの中に描かれる。
	/// 軸を出す関節の選択UIも含むためスケルトンを受け取る
	/// </summary>
	void DrawImGui(const Skeleton& skeleton, Options& options);
}
