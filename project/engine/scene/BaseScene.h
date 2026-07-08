#pragma once

class EffectManager;

// シーン基底クラス
class BaseScene {
public: // メンバ関数

	virtual void Initialize();

	virtual void Finalize();

	// deltaTimeは前フレームからの実経過時間[秒]
	virtual void Update(float deltaTime);

	virtual void Draw();

	virtual void DrawImGui() {};

	virtual ~BaseScene();

	void SetEffectManager(EffectManager* effectManager){ effectManager_ = effectManager; }

protected:
	EffectManager* effectManager_ = nullptr;
};

