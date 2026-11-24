#pragma once

class EffectManager;

// シーン基底クラス。SceneManagerはこの型のポインタ(unique_ptr<BaseScene>)だけを持ち、実体の型を知らずに切り替える。
// 初期化・終了・更新・描画は純粋仮想関数にする: 中身の無い「何もしないシーン」に意味は無く、
// 派生クラスが書き忘れたらコンパイルエラーで気付けるようにするため(抽象クラスなのでBaseScene自体は作れない)
class BaseScene {
public: // メンバ関数

	virtual void Initialize() = 0;

	virtual void Finalize() = 0;

	// deltaTimeは前フレームからの実経過時間[秒]
	virtual void Update(float deltaTime) = 0;

	virtual void Draw() = 0;

	virtual void DrawImGui() {};

	virtual ~BaseScene();

	void SetEffectManager(EffectManager* effectManager){ effectManager_ = effectManager; }

protected:
	// 派生シーンがポストエフェクトを探して操作するための窓口(差し替えはSceneManagerがSetEffectManagerで行う)
	EffectManager* GetEffectManager() const { return effectManager_; }

private:
	EffectManager* effectManager_ = nullptr;
};

