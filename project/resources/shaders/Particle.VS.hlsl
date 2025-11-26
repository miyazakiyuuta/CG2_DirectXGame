#include"Particle.hlsli"

StructuredBuffer<ParticleForGPU> gParticle : register(t0);

VertexShaderOutput main(VertexShaderInput input, uint32_t instanceId : SV_InstanceID) {
    VertexShaderOutput output;
    //output.position = input.position;
    output.position = mul(input.position, gParticle[instanceId].WVP);
    output.texcoord = input.texcoord;
    output.color = gParticle[instanceId].color;
    //output.normal = normalize(mul(input.normal, (float3x3) gTransformationMatrices[instanceId].World));
    return output;
}