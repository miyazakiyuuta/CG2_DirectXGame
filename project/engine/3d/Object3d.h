#pragma once
#include "base/DirectXCommon.h"
#include "math/Matrix4x4.h"
#include "math/Vector3.h"
#include "math/Vector4.h"
#include "math/Transform.h"
#include "3d/AnimationPlayer.h"
#include "3d/Model.h"
#include "3d/Skeleton.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

class Object3dCommon;
class Camera;

// 3Dオブジェクト
class Object3d {
public: // メンバ関数
	void Initialize(Object3dCommon* object3dCommon);
	void Update(float deltaTime);
	void Draw();
	~Object3d();

	// スキン付きモデルを描画できる状態か(Object3dCommonのスキニング一括ディスパッチが見る)
	bool HasSkin() const;
	// スキニングCSを積む(Object3dCommon::DispatchSkinningAllから呼ばれる。直接呼ぶ必要はない)
	void DispatchSkinning(ID3D12GraphicsCommandList* commandList);
	ID3D12Resource* GetSkinnedVertexResource() const { return skinInstance_.skinnedVertexResource.Get(); }

	void PlayAnimation(const std::string& name, bool loop = true, float blendDuration = 0.2f);
	void StopAnimation(float blendDuration = 0.0f) {
		if (!animationPlayer_) return;
		if (blendDuration > 0.0f) {
			animationPlayer_->StopWithBlend(skeleton_, blendDuration);
		} else {
			animationPlayer_->Stop();
		}
	}
	void PauseSwitchingAnimation() {
		if (animationPlayer_) {
			animationPlayer_->SetPause(!animationPlayer_->GetIsPaused());
		}
	}
	void RestartAnimation() {
		if (animationPlayer_) animationPlayer_->Restart();
	}

	/// <summary>
	/// 別のObject3dのJointに追従させる(武器を手に持たせる等)。
	/// 以降このオブジェクトのtransform_は「親Jointから見たローカル変換」として扱われ、
	/// 親のアニメーションに自動で追従する。
	/// 親のUpdateがこのオブジェクトのUpdateより先に呼ばれている必要がある
	/// (親の「今フレームのポーズ」を読むため。1フレーム遅れると武器だけ手から遅れて見える)
	/// </summary>
	/// <param name="parent">追従先のObject3d。nullptrで解除</param>
	/// <param name="jointName">追従先のJoint名(例:"mixamorig:RightHand")</param>
	void SetJointParent(const Object3d* parent, const std::string& jointName);

public: // セッター&ゲッター
	// setter
	void SetModel(const std::string& filePath);
	void SetScale(const Vector3& scale) { transform_.scale = scale; }
	void SetRotate(const Vector3& rotate) { transform_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { transform_.translate = translate; }
	void SetCamera(Camera* camera) { camera_ = camera; }
	void SetColor(Vector4 color) { materialData_->color = color; }
	void SetLightColor(Vector4 color) { directionalLightData_->color = color; }
	void SetLightDirection(Vector3 direction) { directionalLightData_->direction = direction; }
	void SetLightIntensity(float intensity) { directionalLightData_->intensity = intensity; }
	void SetEnableLighting(bool enable) { materialData_->enableLighting = enable; }
	void SetUseEnvironmentMap(bool use) { materialData_->useEnvironmentMap = use; }
	void SetDissolve(float dissolve) { materialData_->dissolve = dissolve; }
	void SetAnimationPause(bool pause) {
		if (animationPlayer_) animationPlayer_->SetPause(pause);
	}

	// getter
	Transform& GetTransform() { return transform_; }
	const Vector3& GetScale() const { return transform_.scale; }
	const Vector3& GetRotate() const { return transform_.rotate; }
	const Vector3& GetTranslate() const { return transform_.translate; }
	Vector4 GetLightColor() const { return directionalLightData_->color; }
	Vector3 GetLightDirection() const { return directionalLightData_->direction; }
	float GetLightIntensity() const { return directionalLightData_->intensity; }
	float GetAnimationDuration(const std::string& name) {
		const auto& animations = model_->GetAnimations();
		auto it = animations.find(name);
		if (it != animations.end()) {
			return it->second.duration;
		}
		return 0.0f;
	}
	int GetAnimationTotalFrames() const {
		if (!animationPlayer_) return 0;
		return animationPlayer_->GetTotalFrames();
	}
	int GetAnimationCurrentFrame() const {
		if (!animationPlayer_) return 0;
		return animationPlayer_->GetCurrentFrame();
	}
	float GetAnimationProgress() const {
		if (!animationPlayer_) return 0.0f;
		return animationPlayer_->GetProgress();
	}
	bool GetIsAnimationFinished() const {
		if (!animationPlayer_) return false;
		return animationPlayer_->IsFinished();
	}
	bool GetIsAnimationPaused() const {
		if (!animationPlayer_) return false;
		return animationPlayer_->GetIsPaused();
	}
	// 指定したボーン名のワールド行列を取得（見つからなければnullopt）
	std::optional<Matrix4x4> GetBoneWorldMatrix(const std::string& boneName) const;
	// 位置だけ欲しい場合のショートカット。
	// 注意: mixamoのようなリグではHandのJointは「手首」にあり、握り位置(手のひら)ではない。
	// 手に物を持たせたり手から何かを出す場合は GetJointAttachPosition を使う
	std::optional<Vector3> GetBoneWorldPosition(const std::string& boneName) const;

	/// <summary>
	/// ジョイントに物を取り付けるための行列。
	/// GetBoneWorldMatrixと違い、リグ内部の単位スケール(glTFのArmatureが持つ0.01など)を
	/// 取り除き、代わりにこのオブジェクト自身のスケールを乗せてある。
	/// SetJointParent が内部で使うものと同一なので、武器のオフセットと
	/// パーティクルの発生位置に同じ数値を渡せば同じ場所になる
	/// </summary>
	std::optional<Matrix4x4> GetJointAttachMatrix(const std::string& boneName) const;

	/// <summary>
	/// ジョイントのローカルオフセットからワールド座標を求める(手のひらの位置など)
	/// </summary>
	/// <param name="localOffset">ジョイントから見たオフセット[m]。
	/// mixamoのHandなら+Yが指先方向なので、{0, 0.07f, 0.02f} 程度で手のひらに乗る</param>
	std::optional<Vector3> GetJointAttachPosition(const std::string& boneName, const Vector3& localOffset) const;
	// 直近のUpdateで確定したワールド行列(ジョイント追従の結果もここに入っている)
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	// スケルトン(デバッグ表示用)。アニメーションを持たないモデルではjointsが空
	const Skeleton& GetSkeleton() const { return skeleton_; }
	// ジョイント名の一覧(ImGuiの選択UI用)。jointMapのキーなので名前順に並ぶ
	std::vector<std::string> GetJointNames() const;

private: // メンバ構造体

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		int32_t useEnvironmentMap;
		float dissolve;
		float padding[1];
		Matrix4x4 uvTransform;
		float shininess;
		float padding1[3];
	};
	
	// 座標変換行列データ
	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
		Matrix4x4 WorldInverseTranspose;
	};

	struct DirectionalLight {
		Vector4 color; //!< ライトの色
		Vector3 direction; //!< ライトの向き
		float intensity; //!< 輝度
	};

private: // メンバ関数

	void CreateMaterialData();
	void CreateTransformationMatrixData();
	void CreateDirectionalLightData();
	// スケルトンの現在ポーズを個体別パレットへ書き込む
	void UpdatePalette();

private: // メンバ変数

	Object3dCommon* object3dCommon_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_;
	TransformationMatrix* transformationMatrixData_ = nullptr;
	
	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_;
	DirectionalLight* directionalLightData_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
	Material* materialData_ = nullptr;
	
	Transform transform_;
	// Updateで確定したワールド行列。ジョイント追従中は「ローカル変換×親Jointのワールド行列」。
	// GetBoneWorldMatrix/デバッグ表示が同じ行列を参照できるよう保持しておく
	Matrix4x4 worldMatrix_ = Matrix4x4::Identity();

	// ジョイント追従(武器の装備など)。未設定ならnullptr
	const Object3d* jointParent_ = nullptr;
	std::string jointParentName_;

	Camera* camera_ = nullptr;

	Model* model_ = nullptr;

	std::unique_ptr<AnimationPlayer> animationPlayer_;
	Skeleton skeleton_;
	// スキニングの個体別資産(パレット・スキン済み頂点)。共有部はModelが持つ
	Model::SkinClusterInstance skinInstance_;
};