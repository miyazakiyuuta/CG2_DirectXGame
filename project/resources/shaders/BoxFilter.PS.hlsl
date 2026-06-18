#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct BoxFilterParam {
    int kernelRadius; // K。3x3ならK=1、5x5ならK=2
    float intensity;  // 0:元画像 1:ぼかし結果
};
ConstantBuffer<BoxFilterParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // 元の色（中心画素）。intensityブレンド用に取っておく
    float4 center = gTexture.Sample(gSampler, input.texcoord);

    // 1テクセル分のuv移動量を算出（Textureの幅・高さの逆数）
    // GetDimensionsはメインメモリへアクセスする場合があるので、
    // 速度が問題になったらCBufferでtexelSizeを渡す方式に切り替える
    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 uvStepSize = float2(rcp((float)width), rcp((float)height));

    int K = gParam.kernelRadius;

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    int count = 0;

    // 動的ループ：カーネルサイズKを実行時に変えられる。
    // [loop]を付けて「展開せず本物のループとして実行」を明示する。
    // 端の処理はSamplerがCLAMPなので、はみ出したuvは端のテクセルに丸められる。
    [loop]
    for (int x = -K; x <= K; ++x) {
        [loop]
        for (int y = -K; y <= K; ++y) {
            float2 offset = float2((float)x, (float)y) * uvStepSize;
            sum += gTexture.Sample(gSampler, input.texcoord + offset).rgb;
            count += 1;
        }
    }

    // BoxFilterは全ての重みが等しいので「合計 ÷ 個数」が平均になる
    // count = (2K+1) * (2K+1)。K=0のときcount=1でぼかし無効（元画像のまま）
    float3 blurred = sum / (float)count;

    // intensityで元画像とブレンド（Kを変えずにぼかし強度を調整できる）
    output.color.rgb = lerp(center.rgb, blurred, gParam.intensity);
    output.color.a = center.a;
    return output;
}
