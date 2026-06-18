#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct VignetteParam {
    float3 color; // ヴィネット色（黒=従来の暗化、色を入れると着色）
    float intensity; // 効果の強さ 0..1（0で無効）
    float radius; // 暗くし始める中心からの正規化距離
    float softness; // 減衰帯の幅
    float power; // 落ち方のカーブ
    float aspect; // 1=楕円(補正なし) / 画面比を入れると円に寄る
};
ConstantBuffer<VignetteParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);

    // 中心を原点に
    float2 d = input.texcoord - 0.5f;
    // アスペクト補正（aspect=1で補正なし＝楕円、画面比で円に寄る）
    d.x *= gParam.aspect;
    float dist = length(d);

    // radius より内側は1.0（明るさ維持）、radius+softness で0へ
    float v = 1.0f - smoothstep(gParam.radius, gParam.radius + gParam.softness, dist);
    // 落ち方のカーブ調整
    v = pow(saturate(v), gParam.power);

    // 強さ。intensity=0でfactor=1（無効）
    float factor = lerp(1.0f, v, gParam.intensity);

    // factor=1で元の色、0でvignetteColor（黒なら暗化、色なら着色）
    output.color.rgb = lerp(gParam.color, output.color.rgb, factor);
    return output;
}