#include "ImGuiManager.h"

#ifdef USE_IMGUI
#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>
#endif

void ImGuiManager::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]]  SrvManager* srvManager) {
#ifdef USE_IMGUI
	winApp_ = winApp;
	dxCommon_ = dxCommon;
	srvManager_ = srvManager;

	// ImGuiのコンテキストを設定
	ImGui::CreateContext();
	// ImGuiのスタイルを設定
	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	uint32_t srvIndex = srvManager_->Allocate();
	ImGui_ImplDX12_Init(
		dxCommon_->GetDevice(),
		static_cast<int>(dxCommon_->GetSwapChainResourceNum()),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvManager_->GetDescriptorHeap(),
		srvManager_->GetCPUDescriptorHandle(srvIndex),
		srvManager_->GetGPUDescriptorHandle(srvIndex)
	);

	ImGui::GetIO().Fonts->Build();
#endif
}

void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
	// 後始末
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
	// ImGuiフレーム開始
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX12_NewFrame();
	ImGui::NewFrame();
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
	// 描画前準備
	ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef USE_IMGUI
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	// デスクリプタヒープの配列をセットするコマンド
	ID3D12DescriptorHeap* ppHeaps[] = { srvManager_->GetDescriptorHeap() };
	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
	// 描画コマンドを発行
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}