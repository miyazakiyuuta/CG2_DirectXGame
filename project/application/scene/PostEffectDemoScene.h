#pragma once
#include "scene/BaseScene.h"

#include <memory>
#include <string>

class Camera;
class Object3d;

// CG5評価課題1のポストエフェクト・デモシーン。
// terrain+球など数個のオブジェクトを描画し、数字キーで各エフェクトを排他切替する
// (1:Grayscale 2:Vignette 3:BoxFilter 4:Gaussian 5:LuminanceOutline
//  6:DepthOutline 7:RadialBlur 8:Dissolve 9:Random(Noise) 0:全OFF)
class PostEffectDemoScene : public BaseScene {
public:

	void Initialize() override;

	void Finalize() override;

	void Update(float deltaTime) override;

	void Draw() override;

	void DrawImGui() override;

	PostEffectDemoScene();

	~PostEffectDemoScene() override;

private:
	std::unique_ptr<Camera> camera_ = nullptr;

	std::unique_ptr<Object3d> terrain_;
	std::unique_ptr<Object3d> ball_;
	std::unique_ptr<Object3d> ballFar_;

	// 現在ONのエフェクト名(画面表示用。"None"=全OFF)
	std::string activeEffectName_ = "None";
};
