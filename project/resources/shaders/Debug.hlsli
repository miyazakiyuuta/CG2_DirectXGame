struct TransformationMatrix {
    float4x4 matWVP;
    float4 color;
};

ConstantBuffer<TransformationMatrix> gData : register(b0);

struct VertexShaderInput {
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

struct VertexShaderOutput {
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};