#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct RadialBlurParam {
    float2 center; // 放射の中心（UV, 0..1）
    float blurWidth; // 1サンプルあたりの進み幅
    float intensity; // 全体の強さ 0..1（0で無効）
    int numSamples; // サンプル数
    float focusRadius; // ここまではぼかさない（中心からの正規化距離）
    float focusSoftness; // 減衰帯の幅
    float aspect; // 1=楕円 / 画面比でフォーカスを円に寄せる
};
ConstantBuffer<RadialBlurParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    float2 uv = input.texcoord;
    float3 original = gTexture.Sample(gSampler, uv).rgb;

    // 中心から外向きの方向。中心から遠いほど長い＝外側ほど強くぼける
    float2 dir = uv - gParam.center;

    int n = clamp(gParam.numSamples, 1, 64); // 0除算と暴走を防ぐ
    float3 sum = float3(0.0f, 0.0f, 0.0f);
    [loop]
    for (int i = 0; i < n; ++i) {
        // 資料の式：現在UVから方向へ i 段階ずつ進めてサンプリング
        float2 sampleUV = uv + dir * gParam.blurWidth * float(i);
        sum += gTexture.Sample(gSampler, sampleUV).rgb;
    }
    float3 blurred = sum * rcp(float(n)); // 単純平均

    // フォーカスマスク：中心付近を鮮明に保つ（focusRadius=0でほぼ均一）
    float2 d = uv - gParam.center;
    d.x *= gParam.aspect;
    float dist = length(d);
    float mask = smoothstep(gParam.focusRadius, gParam.focusRadius + gParam.focusSoftness, dist);

    float3 focused = lerp(original, blurred, mask); // 中心=original / 外側=blurred
    float3 result = lerp(original, focused, gParam.intensity); // 全体の強さ

    output.color = float4(result, 1.0f);
    return output;
}