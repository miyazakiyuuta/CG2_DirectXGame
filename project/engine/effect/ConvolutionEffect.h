#pragma once
#include "effect/IPostEffect.h"

#include <wrl/client.h>
#include <d3d12.h>
#include <cstddef>

// 畳み込み系ポストエフェクトの共通基底。
// ルートシグネチャ・PSO・CBufferは基底が非公開で持ち、派生クラスには
// 「CBufferを作る」「描く」「デバイス類を借りる」の protected 関数だけを渡す。

class ConvolutionEffect : public IPostEffect {
public:
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) override;
    void Draw(uint32_t srcSrvIndex) override;

protected:
    // 名前は派生クラスから受け取り、そのままIPostEffectへ渡す
    explicit ConvolutionEffect(const char* name) : IPostEffect(name) {}

    // 派生クラスが実装するフック
    virtual const wchar_t* GetPixelShaderPath() const = 0;
    virtual void InitializeParam() = 0; // CreateParamBufferを呼び、初期値を書き込む

    // 派生クラスが自分のCBufferを作るためのヘルパー。
    // 基底のDrawがb0に積むCBufferを sizeof(T) で作り、マップ先を T* で返す
    // (型はここで決まるので、派生側でvoid*を読み替えるキャストが要らない)
    template <class T>
    T* CreateParamBuffer() { return static_cast<T*>(CreateParamBufferRaw(sizeof(T))); }

    // 共通のルートシグネチャ・PSOで、srcSrvIndexをt0・paramAddressをb0にしてフルスクリーン三角形を描く。
    // 描画先(RTV)は呼び出し側がBeginRender済みであること
    void DrawFullscreen(uint32_t srcSrvIndex, D3D12_GPU_VIRTUAL_ADDRESS paramAddress);

    // 派生クラスが独自のCBufferや中間RTを作るとき用(借りるだけで、付け替えはできない)
    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SrvManager* GetSrvManager() const { return srvManager_; }

private:
    // CreateParamBuffer<T>の本体。paramResource_を作ってマップし、マップ先を返す
    void* CreateParamBufferRaw(size_t size);
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

    // CreateParamBufferで作るCBuffer(マップ先は派生クラスが型付きで持つ)
    Microsoft::WRL::ComPtr<ID3D12Resource> paramResource_ = nullptr;
};
