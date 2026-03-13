struct TransformationMatrix {
    float4x4 matWVP;
    float4 color;
};

// ConstantBuffer<ConstantBufferData> gConstBuffer : register(b0);
cbuffer gConstantBuffer : register(b0) {
    TransformationMatrix gData[128];
}

struct VertexShaderInput {
    float3 pos : POSITION;
};

struct VertexShaderOutput {
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};