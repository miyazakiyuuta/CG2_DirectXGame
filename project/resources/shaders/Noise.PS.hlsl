#include "Fullscreen.hlsli"
#include "Random.hlsl"

Texture2D<float4> gTexture : register(t0); // シーン色
SamplerState gSampler : register(s0);

struct NoiseParam {
    float time;      // 経過時間。SeedをずらしてフレームごとにノイズをシャッフルするためCPUから積算して渡す
    float intensity; // Grainモードでシーンに加算するノイズの強度
    int   mode;      // 0=Pure(乱数のみ出力) / 1=Grain(シーン色に重ねる)
    float pad;       // 16byte境界合わせ
};
ConstantBuffer<NoiseParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // Seedはピクセルごとのtexcoord。これだけだと毎フレーム固定なので、
    // 経過時間を加算してSeedをずらす（加算なら空間周波数を保ったまま再シャッフルされる）。
    float n = rand2dTo1d(input.texcoord + gParam.time);

    if (gParam.mode == 0) {
        // Pure: グレースケール乱数をそのまま出力（資料01-07の確認モード）
        output.color = float4(n, n, n, 1.0f);
    } else {
        // Grain: 0.5中心の符号付きノイズを加算（平均輝度を保つフィルムグレイン）
        float4 scene = gTexture.Sample(gSampler, input.texcoord);
        scene.rgb += (n - 0.5f) * gParam.intensity;
        output.color = scene;
    }
    return output;
}
