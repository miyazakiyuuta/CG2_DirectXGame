#include "Skybox.hlsli"
#include "GammaCorrection.hlsli"

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};
TextureCube<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    output.color = gTexture.Sample(gSampler, input.texcoord);
    output.color.rgb = LinearToSRGB(output.color.rgb);
    return output;
}