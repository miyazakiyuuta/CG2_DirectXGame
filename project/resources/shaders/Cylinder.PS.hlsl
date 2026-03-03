#include "Cylinder.hlsli"

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    float2 texcoord = input.texcoord;
    texcoord.y = 1.0f - texcoord.y;
    float4 transformedUV = mul(float4(texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    output.color = gMaterial.color * textureColor * input.color;
    
    if (textureColor.a == 0.0f) {
        discard;
    }
    
    return output;
}