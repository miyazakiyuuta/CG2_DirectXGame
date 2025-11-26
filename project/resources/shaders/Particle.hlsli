#define int32_t int
#define uint32_t uint

struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float4 color : COLOR0;
};

struct TransformationMatrix {
    float4x4 WVP;
    float4x4 World;
};

struct VertexShaderInput {
    float4 position : POSITION0;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
};

struct ParticleForGPU {
    float4x4 WVP;
    float4x4 World;
    float4 color;
};