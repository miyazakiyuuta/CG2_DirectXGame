#include"Skybox.hlsli"

struct TransformationMatrix {
    float4x4 WVP;
};
ConstantBuffer<TransformationMatrix> gTransformationMatrix : register(b0);
struct VertexShaderInput {
    float4 position : POSITION0;
};

VertexShaderOutput main(VertexShaderInput input) {
    VertexShaderOutput output;
    output.position = mul(input.position, gTransformationMatrix.WVP).xyww;
    output.texcoord = input.position.xyz;
    return output;
}