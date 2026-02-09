#pragma once

// 前方宣言
class SceneManager;

// シーン基底クラス
class BaseScene {
public: // メンバ関数

	virtual void Initialize();

	virtual void Finalize();

	virtual void Update();

	virtual void Draw();

	virtual ~BaseScene() = default;
};

