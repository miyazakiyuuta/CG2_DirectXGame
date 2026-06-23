#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);
struct GaussianParam {

    int kernelRadius; // K。3x3ならK=1、7x7ならK=3
    float sigma; // 標準偏差σ
    float intensity; // 0:元画像 1:ぼかし結果
    int direction; // 0:横パス 1:縦パス
};
ConstantBuffer<GaussianParam> gParam : register(b0);

static const float PI = 3.14159265f;

// 1次元ガウス関数。分離フィルタなので2Dではなく1Dで重みを出す。
// 横パスでg(x)、縦パスでg(y)を掛けると、合成でg(x)*g(y)＝2Dガウスになる。
float gauss1D(float d, float sigma) {
    float exponent = -(d * d) * rcp(2.0f * sigma * sigma);
    // この分母(sqrt(2π)σ)はループ内で定数なので、後段のweightSum正規化で打ち消える。
    // テキストの2Dガウスと対応づけるため一応残しているだけ。
    float denominator = sqrt(2.0f * PI) * sigma;
    return exp(exponent) * rcp(denominator);
}

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float4 center = gTexture.Sample(gSampler, input.texcoord); // intensityブレンド用

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texel = float2(rcp((float) width), rcp((float) height));

    // ★分離の肝：横パス=(1,0)、縦パス=(0,1)。1パスにつき1軸しか動かない。
    float2 axis = (gParam.direction == 0) ? float2(1.0f, 0.0f) : float2(0.0f, 1.0f);

    int K = gParam.kernelRadius;
    float sigma = max(gParam.sigma, 1e-4f); // 0除算防止

    float3 sum = float3(0.0f, 0.0f, 0.0f);
    float weightSum = 0.0f;

    [loop]
    for (int i = -K; i <= K; ++i) {
        float w = gauss1D((float) i, sigma);
        float2 offset = axis * texel * (float) i;
        sum += gTexture.Sample(gSampler, input.texcoord + offset).rgb * w;
        weightSum += w;
    }

    // 重み合計で正規化（合計が1になるよう底上げ。BoxFilterのcount除算のガウス版）
    float3 blurred = sum / max(weightSum, 1e-6f);

    // 横・縦の両パスで同じlerpを掛ける。これにより
    //   intensity=0 → 素通り（元画像が両パスを通過して保存される）
    //   intensity=1 → 完全な2Dガウスぼかし
    // になる（中間値は厳密な線形ではないがスライダーの体感は自然）。
    output.color.rgb = lerp(center.rgb, blurred, gParam.intensity);
    output.color.a = center.a;
    return output;
}