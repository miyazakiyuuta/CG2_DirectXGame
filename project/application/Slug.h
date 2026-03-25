#pragma once

#include "math/Vector3.h"
#include "math/Vector4.h"
#include <memory>
#include <string>
#include <vector>

// 前方宣言
class Camera;
class Object3d;
class Object3dCommon;
class DirectXCommon;
class DebugGrid; // グリッド表示クラスを使用

/// <summary>
/// ナメクジの挙動と、地面に残る「グリッド状の跡」を管理するクラス
/// </summary>
class Slug {
public:
	/// <summary>
	/// 軌跡（跡）ひとつ分を管理する構造体
	/// </summary>
	struct TrailSegment {
		std::unique_ptr<DebugGrid> grid; // 球体の代わりにDebugGridを保持
		float lifespan;                  // 寿命（1.0 -> 0.0）
	};

	Slug();
	~Slug();

	/// <summary>
	/// 初期化
	/// </summary>
	/// <param name="dxCommon">グリッドの生成に必要</param>
	void Initialize(DirectXCommon* dxCommon, Object3dCommon* object3dCommon, Camera* camera, const std::string& bodyModel);

	/// <summary>
	/// 更新処理
	/// </summary>
	void Update(float deltaTime);

	/// <summary>
	/// 本体（不透明）の描画
	/// </summary>
	void Draw();

	/// <summary>
	/// 軌跡（半透明グリッド）の描画
	/// </summary>
	void DrawTransparent(const Camera& camera);

	// セッター
	void SetPosition(const Vector3& pos);
	void SetBodyColor(const Vector4& color);
	void SetTrailColor(const Vector4& color);

private:
	/// <summary>
	/// 新しいグリッドの跡を生成
	/// </summary>
	void AddTrail();

private:
	DirectXCommon* dxCommon_ = nullptr;
	Object3dCommon* object3dCommon_ = nullptr;
	Camera* camera_ = nullptr;

	// ナメクジ本体
	std::unique_ptr<Object3d> body_;
	Vector3 position_;
	float yaw_;
	float speed_;

	// 色設定
	Vector4 bodyColor_;
	Vector4 trailColor_;

	// 制御用
	float timer_;
	Vector3 lastTrailPos_;
	std::vector<TrailSegment> trails_;

	// 定数
	const float kTrailInterval;
	const float kTrailFadeSpeed;
	const float kSnakeIntensity;
};