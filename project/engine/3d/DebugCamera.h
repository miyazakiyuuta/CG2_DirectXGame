#pragma once
#include "3d/Camera.h"
#include "math/Vector3.h"

#include <memory>

/// <summary>
/// デバッグカメラ(エディタ用フライスルーカメラ)
/// 専用のCameraを所有し、有効中はシーンが描画用カメラをこちらへ差し替える。
/// ゲームカメラのTransformには一切書き込まない(JSON保存を汚さない)。
///
/// 操作(Sceneビュー上でのみ受け付け):
///   右ドラッグ         : 視点回転
///   右ドラッグ + WASD/QE: 前後左右上下移動(Shiftで高速)
///   右ドラッグ + ホイール: 移動速度の増減
///   中ドラッグ         : パン(上下左右の平行移動)
///   ホイール           : 前後ドリー
/// </summary>
class DebugCamera {
public:
	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="gameCamera">ON時の視点同期と視点コピーの相手になるゲームカメラ</param>
	void Initialize(Camera* gameCamera);

	/// <summary>
	/// 更新(無効中は何もしない)
	/// </summary>
	void Update(float deltaTime);

	/// <summary>
	/// ON/OFFチェックボックスと速度・感度の調整UI(Debug構成のみ)
	/// </summary>
	void DrawImGui();

	/// ON/OFF切替。初回ONのみゲームカメラの視点から開始し、以降は前回のデバッグ視点を維持する
	void SetActive(bool active);
	bool IsActive() const { return isActive_; }

	/// 描画に使うカメラ(有効中にシーンが差し替えて使う)
	Camera* GetCamera() const { return camera_.get(); }

	/// 対象を現在の視線方向のまま正面に捉える位置へ移動(Fキーフォーカス)
	void FocusOn(const Vector3& target, float radius);

	/// 現在の視点をゲームカメラのTransformへ書き込む(構図の本番採用用)
	void CopyPoseToGameCamera();

private:
	// ゲームカメラの視点・画角を引き継ぐ
	void SyncPoseFrom(const Camera& src);

	std::unique_ptr<Camera> camera_;
	Camera* gameCamera_ = nullptr;

	Vector3 position_ = { 0.0f, 7.5f, -20.0f };
	float yaw_ = 0.0f;   // Y軸回りの回転角
	float pitch_ = 0.0f; // X軸回りの回転角(真上・真下の手前でクランプ)

	bool isActive_ = false;
	bool hasActivatedOnce_ = false; // 一度でもONにしたか(初回のみ視点同期するため)
	bool isFlying_ = false;  // 右ドラッグ操作中か(Sceneビュー上で開始した場合のみtrue)
	bool isPanning_ = false; // 中ドラッグ操作中か(同上)

	float moveSpeed_ = 5.0f;           // 移動速度(units/秒)
	float rotateSensitivity_ = 0.003f; // 回転感度(rad/ピクセル)
	float shiftMultiplier_ = 3.0f;     // Shift押下時の速度倍率
};
