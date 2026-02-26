#pragma once
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

// ImGuiの管理
class ImGuiManager {
public:
	void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);

	void Finalize();

	/// <summary>
	/// ImGui受付開始
	/// </summary>
	void Begin();

	/// <summary>
	/// ImGui受付終了
	/// </summary>
	void End();

	/// <summary>
	/// 画面への描画
	/// </summary>
	void Draw();

private:
	WinApp* winApp_;
	DirectXCommon* dxCommon_;
	SrvManager* srvManager_;
};