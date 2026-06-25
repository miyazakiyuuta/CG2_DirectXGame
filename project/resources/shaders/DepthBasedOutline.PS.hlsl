#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0); // 元画像(カラー)
Texture2D<float> gDepthTexture : register(t1); // 深度(NDC)
SamplerState gSampler : register(s0); // カラー用 Linear
SamplerState gSamplerPoint : register(s1); // 深度用 Point

struct OutlineParam {
    float4x4 projectionInverse;
    float3 lineColor;
    float strength;
    float threshold;
    int debugView;
    float2 _pad;
};
ConstantBuffer<OutlineParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

// 横方向Prewitt
static const float kPrewittH[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};
// 縦方向Prewitt
static const float kPrewittV[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    uint w, h;
    gTexture.GetDimensions(w, h); // texelサイズは既存流儀でここから
    float2 uvStep = float2(rcp((float) w), rcp((float) h));

    float2 difference = float2(0.0f, 0.0f);

    [unroll]
    for (int x = 0; x < 3; ++x) {
        [unroll]
        for (int y = 0; y < 3; ++y) {
            float2 uv = input.texcoord + float2(x - 1, y - 1) * uvStep;

            float ndcDepth = gDepthTexture.Sample(gSamplerPoint, uv); // 補間禁止=Point
            // NDC → View（x,yはz,wに効かないので0でよい）
            float4 viewSpace = mul(float4(0.0f, 0.0f, ndcDepth, 1.0f), gParam.projectionInverse);
            float viewZ = viewSpace.z * rcp(viewSpace.w); // 同次→デカルト

            difference.x += viewZ * kPrewittH[x][y];
            difference.y += viewZ * kPrewittV[x][y];
        }
    }

    float weight = length(difference) * gParam.strength;
    weight = (weight < gParam.threshold) ? 0.0f : weight; // しきい値カット
    weight = saturate(weight);

    float3 sceneColor = gTexture.Sample(gSampler, input.texcoord).rgb;

    if (gParam.debugView != 0) { // 調整用：白いほどエッジ
        output.color = float4(weight.xxx, 1.0f);
        return output;
    }
    output.color.rgb = lerp(sceneColor, gParam.lineColor, weight); // 線色合成
    output.color.a = 1.0f;
    return output;
}