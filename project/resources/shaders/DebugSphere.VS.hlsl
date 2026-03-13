#include "DebugSphere.hlsli"

VertexShaderOutput main(VertexShaderInput input, uint instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    output.svpos = mul(float4(input.pos, 1.0f), gData[instanceId].matWVP);
    output.color = gData[instanceId].color;
    return output;
}