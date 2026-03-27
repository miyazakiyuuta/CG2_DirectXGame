#include "Debug.hlsli"

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    output.svpos = mul(float4(input.pos, 1.0f), gData.data[instanceId].matWVP);
    output.color = input.color * gData.data[instanceId].color;
    output.uv = input.uv;
    return output;
}