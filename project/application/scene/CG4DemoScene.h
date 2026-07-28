#pragma once
#include "scene/BaseScene.h"
#include "debug/SkeletonDebug.h"
#include "math/Vector3.h"
#include "math/Transform.h"

#include <memory>
#include <string>

class Camera;
class DebugCamera;
class Object3d;
class ParticleEmitter;
class GPUParticleEmitter;

class CG4DemoScene : public BaseScene {
public:

	CG4DemoScene();
	~CG4DemoScene() override;

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	void DrawImGui() override;

private:

	// 入力(パッド左スティック / WASD)からXZ平面の移動方向を作る。長さは0〜1
	Vector3 ReadMoveInput() const;

	// 移動量に応じて再生アニメーションを切り替える(切り替えはブレンド付き)
	void UpdateAnimationState(float moveAmount);

	// 描画に使うカメラ(デバッグカメラON中はそちらを返す)
	Camera* GetActiveCamera() const;

	// キャラクタを追いかけるカメラの位置と向きを更新する
	void UpdateFollowCamera();

private:

	std::unique_ptr<Camera> camera_;
	std::unique_ptr<DebugCamera> debugCamera_;

	// 追従カメラ: 注視点の高さ、後方距離、見下ろし角[rad]。
	// カメラのfovYは0.45rad(約26度)と狭いので、身長1.66mが画面の半分程度に収まる距離を既定にする
	float cameraHeight_ = 0.9f;
	float cameraDistance_ = 7.0f;
	float cameraPitch_ = 0.1f;
	bool followCameraActive_ = true;

	// キャラクタの向きに対するカメラの周回角[rad]。0で真後ろ、πで正面。
	// これが無いとカメラが常に背中側へ回り込むため、正面から見られない
	float cameraOrbitYaw_ = 0.0f;
	float cameraOrbitSpeed_ = 2.2f; // 周回の速さ[rad/秒]

	// 見た目の確認中はキャラクタを原点に固定しておきたいことがある
	bool lockPosition_ = false;

	// スキニングキャラクタ本体
	std::unique_ptr<Object3d> human_;
	// 手に持たせる武器。human_のJointへ親子付けするのでtransformはJointローカル
	std::unique_ptr<Object3d> hammer_;
	// 地面
	std::unique_ptr<Object3d> ground_;

	// キャラクタの位置とY軸回りの向き[rad](移動入力から作る)
	Vector3 characterPosition_{ 0.0f, 0.0f, 0.0f };
	float characterAngle_ = 0.0f;
	float moveSpeed_ = 2.6f;       // 移動速度[m/秒]
	float turnLerpRate_ = 12.0f;   // 向きの追従の速さ[1/秒]
	float currentMoveAmount_ = 0.0f; // 直近の入力量(0〜1)。アニメ切り替えの判定に使う

	// アニメーション補間
	std::string currentAnimationName_;
	float blendDuration_ = 0.25f;  // 切り替えにかける時間[秒]
	float walkThreshold_ = 0.65f;  // これ以上の入力量で sneakWalk → walk へ切り替える
	bool autoAnimationSwitch_ = true;

	// 骨のデバッグ表示。
	// DebugRendererの線は深度テストありなので、骨はメッシュの内側に隠れて見えない。
	// 骨だけを確認したいときのためにメッシュを消せるようにしてある
	// (深度テストを切ると他シーンのグリッドやレール表示にも影響するため共有側は変えない)
	SkeletonDebug::Options skeletonOptions_;
	bool drawSkeleton_ = true;
	bool hideMesh_ = false;

	// 武器の装備
	bool equipWeapon_ = true;
	std::string weaponJointName_;
	// 手のJointから見た武器のローカル変換。モデルの原点や軸の向きのズレをここで吸収する
	Transform weaponOffset_{};

	// 手から出すパーティクル
	std::unique_ptr<GPUParticleEmitter> handAuraEmitter_; // 左手から常時放出
	std::unique_ptr<ParticleEmitter> handBurstEmitter_;   // 右手から単発放出
	std::string auraJointName_;
	std::string burstJointName_;
	// 手のJointから見た発生位置のオフセット。HandのJointは手首にあるので、
	// そのまま使うと手首から出てしまう。武器のオフセットと同じ基準の値
	Vector3 handParticleOffset_{};
	bool handAuraActive_ = true;
	uint32_t burstCount_ = 40;
};
