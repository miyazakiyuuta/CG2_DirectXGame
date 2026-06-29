#include "Fullscreen.hlsli"

Texture2D<float4> gTexture     : register(t0); // シーン色
Texture2D<float4> gMaskTexture : register(t1); // maskノイズ(グレースケール画像。redを参照)
SamplerState gSampler     : register(s0); // シーン色 Linear/Clamp
SamplerState gMaskSampler : register(s1); // mask Linear/Wrap

struct DissolveParam {
    float3 edgeColor;
    float  threshold;
    float3 holeColor;
    float  edgeWidth;
    float2 maskTiling;
    float2 _pad;
};
ConstantBuffer<DissolveParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    // ノイズはRGBA画像なのでredを濃淡値として使う
    float  mask  = gMaskTexture.Sample(gMaskSampler, input.texcoord * gParam.maskTiling).r;
    float4 scene = gTexture.Sample(gSampler, input.texcoord);

    // 境界(threshold〜threshold+edgeWidth)付近で1になる縁取り
    float  edge = 1.0f - smoothstep(gParam.threshold, gParam.threshold + gParam.edgeWidth, mask);
    float3 lit  = scene.rgb + edge * gParam.edgeColor; // 溶け残り側に発光を加算

    // mask <= threshold の画素は穴 = holeColor で塗る
    float  hole = step(mask, gParam.threshold);
    float3 rgb  = lerp(lit, gParam.holeColor, hole);

    output.color = float4(rgb, 1.0f);
    return output;
}
