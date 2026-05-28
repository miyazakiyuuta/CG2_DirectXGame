#include "SkyCylinder.hlsli"
#include "GammaCorrection.hlsli"

struct Material {
    float4 color;
    float4x4 uvTransform;
};
ConstantBuffer<Material> gMaterial : register(b0);

Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    float4 uv = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    output.color = gMaterial.color * gTexture.Sample(gSampler, uv.xy);
    output.color.rgb = LinearToSRGB(output.color.rgb);
    return output;
}