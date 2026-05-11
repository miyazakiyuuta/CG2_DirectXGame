#include "DebugSphere.hlsli"
#include "GammaCorrection.hlsli"

float4 main(VertexShaderOutput input) : SV_TARGET {
    float4 color = input.color;
    color.rgb = LinearToSRGB(color.rgb);
    return color;
}