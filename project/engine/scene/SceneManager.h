#pragma once
#include "scene/BaseScene.h"
#include "scene/AbstractSceneFactory.h"
#include "scene/ITransition.h"
#include <memory>

class EffectManager;

class SceneManager {
public:
	static SceneManager* GetInstance();
	// 最終シーンの終了処理とシングルトンの破棄(Framework::Finalizeが呼ぶ)
	static void Finalize();

	void Update(float deltaTime);

	// シーンの描画(遷移は含まない)
	void Draw();

	// 遷移の描画。画面全体を覆うものなので、パーティクルも描き終えた最後に呼ぶ(Framework::Runが呼ぶ)
	void DrawTransition();

	void DrawImGui();

	// 遷移中(次のシーンを予約済み、または遷移の演出が終わっていない)の間に呼ぶと、何もせずに戻る
	void ChangeScene(const std::string& sceneName, std::unique_ptr<ITransition> transition = nullptr);

	/// <summary>
	/// 遷移中か。シーンが切り替わった直後も、遷移の演出(帯が開く等)が終わるまではtrue。
	/// この間のChangeSceneは無視されるので、シーン側で「次へ進む」入力を受け付けるかの判断に使う
	/// </summary>
	bool IsTransitioning() const { return transition_ != nullptr || nextScene_ != nullptr; }

	void SetSceneFactory(AbstractSceneFactory* factory) { sceneFactory_ = factory; }

	void SetEffectManager(EffectManager* effectManager) {
		effectManager_ = effectManager;
		if (scene_) {
			scene_->SetEffectManager(effectManager);
		}
		if (nextScene_) {
			nextScene_->SetEffectManager(effectManager);
		}
	}

private:
	static SceneManager* instance;

	AbstractSceneFactory* sceneFactory_;

	std::unique_ptr<BaseScene> scene_ = nullptr;
	std::unique_ptr<BaseScene> nextScene_ = nullptr;

	std::unique_ptr<ITransition> transition_ = nullptr;
	bool isSceneChanged_ = false;

	EffectManager* effectManager_ = nullptr;
};

