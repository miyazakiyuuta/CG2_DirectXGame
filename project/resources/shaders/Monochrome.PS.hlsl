#include "Fullscreen.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct MonochromeParam {
    float3 color; // 掛ける色（白=Grayscale、セピア色=Sepia）
    float intensity;
};
ConstantBuffer<MonochromeParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    float value = dot(output.color.rgb, float3(0.2125f, 0.7154f, 0.0721f));
    float3 effected = value * gParam.color;
    output.color.rgb = lerp(output.color.rgb, effected, gParam.intensity);
    return output;
}