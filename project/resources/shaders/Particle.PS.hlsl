#include "Particle.hlsli"
struct Material {
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
};
//ConstantBuffer<Material> gMaterial : register(b0);
cbuffer MaterialCB : register(b0) {
    Material gMaterial;
}

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    //float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    output.color = gMaterial.color * textureColor * input.color;
    
    if (textureColor.a == 0.0f) {
        discard;
    }
    
    return output;
}