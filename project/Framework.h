#pragma once

class WinApp;
class DirectXCommon;
class SrvManager;
class Object3dCommon;
class SpriteCommon;

// ゲーム全体
class Framework {
public: // メンバ関数
	
	// 初期化
	virtual void Initialize();

	// 終了
	virtual void Finalize();

	// 毎フレーム更新
	virtual void Update();

	// 描画
	virtual void Draw();

	// 終了チェック
	virtual bool IsEndRequest() { return isEndRequest_; }

	// 仮想デストラクタ
	virtual ~Framework() = default;

	// 実行
	void Run();
	
public: // ゲッター

	WinApp* GetWinApp() { return winApp_; }
	DirectXCommon* GetDxCommon() { return dxCommon_; }
	SrvManager* GetSrvManager() { return srvManager_; }
	Object3dCommon* GetObject3dCommon() { return object3dCommon_; }
	SpriteCommon* GetSpriteCommon() { return spriteCommon_; }

private:

	bool isEndRequest_ = false;

	WinApp* winApp_ = nullptr;
	DirectXCommon* dxCommon_ = nullptr;
	SrvManager* srvManager_ = nullptr;
	Object3dCommon* object3dCommon_ = nullptr;
	SpriteCommon* spriteCommon_ = nullptr;
};

