#include "Debug.hlsli"

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.svpos = mul(float4(input.pos, 1.0f), gData.matWVP);
    output.color = input.color * gData.color;
    output.uv = input.uv;
    return output;
}