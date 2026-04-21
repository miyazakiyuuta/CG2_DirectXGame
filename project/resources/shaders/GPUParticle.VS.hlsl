#include"Particle.hlsli"

StructuredBuffer<Particle> gParticles : register(t0);
ConstantBuffer<PerView> gPerView : register(b0);

static const float2 kQuadPos[6] = {
    float2(-0.5f, 0.5f), // 左上
    float2(0.5f, 0.5f), // 右上
    float2(-0.5f, -0.5f), // 左下
    float2(0.5f, 0.5f), // 右上
    float2(0.5f, -0.5f), // 右下
    float2(-0.5f, -0.5f), // 左下
};

static const float2 kQuadUV[6] = {
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f),
    float2(0.0f, 1.0f),
};

VertexShaderOutput main(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID) {
    VertexShaderOutput output;

    Particle particle = gParticles[instanceId];
    
    float4x4 worldMatrix = gPerView.billboardMatrix;
    worldMatrix[0] *= particle.scale.x;
    worldMatrix[1] *= particle.scale.y;
    worldMatrix[2] *= particle.scale.z;
    worldMatrix[3].xyz = particle.translate;

    //output.position = mul(input.position, mul(worldMatrix, gPerView.viewProjection));
    //output.texcoord = input.texcoord;
    //output.color = particle.color;
    
    float4 localPos = float4(kQuadPos[vertexId], 0.0f, 1.0f);
    output.position = mul(localPos, mul(worldMatrix, gPerView.viewProjection));
    output.texcoord = kQuadUV[vertexId];
    output.color = particle.color;
    
    return output;
}