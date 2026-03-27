struct TransformationMatrix {
    float4x4 matWVP;
    float4 color;
};

struct ConstBufferData {
    TransformationMatrix data[128];
};

ConstantBuffer<ConstBufferData> gData : register(b0);

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