#include "Debug.hlsli"
#include "GammaCorrection.hlsli"

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

float4 main(VertexShaderOutput input) : SV_TARGET {
    float4 texColor = gTexture.Sample(gSampler, input.uv);
    float4 color = texColor * input.color;
    color.rgb = LinearToSRGB(color.rgb);
    return color;
}