#include "Object3d.hlsli"
struct Material {
    float4 color;
    int enableLighting;
    float4x4 uvTransform;
    float shininess;
};
ConstantBuffer<Material> gMaterial : register(b0);
struct PixelShaderOutput{
    float4 color : SV_TARGET0;
};
Texture2D<float4> gTexture : register(t0);
SamplerState gSampler : register(s0);

struct DirectionalLight {
    float4 color; //!< ライトの色
    float3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;
    
    //float4 textureColor = gTexture.Sample(gSampler, input.texcoord);
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    if (textureColor.a <= 0.5f) {
        discard;
    }
    
    if (gMaterial.enableLighting != 0) { // Lightingする場合
        
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        float3 halfVector = normalize(-gDirectionalLight.direction + toEye);
        float NDotH = dot(normalize(input.normal), halfVector);
        float specularPow = pow(saturate(NDotH), gMaterial.shininess);
        //float3 reflectLight = reflect(gDirectionalLight.direction, normalize(input.normal));
        //float RdotE = dot(reflectLight, toEye);
        //float specularPow = pow(saturate(RdotE), gMaterial.shininess);
        
        
        
        // 拡散反射
        float3 diffuse = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * cos * gDirectionalLight.intensity;
        // 鏡面反射
        float3 specular = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPow * float3(1.0f, 1.0f, 1.0f);
        // 拡散反射 + 鏡面反射
        output.color.rgb = diffuse + specular;
        
        output.color.a = gMaterial.color.a * textureColor.a;
        
        //float cos = saturate(dot(normalize(input.normal), -gDirectionalLight.direction)); // 月のようなライティング
        /*
        float NdotL = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cos = pow(NdotL * 0.5f + 0.5f, 2.0f);
        output.color = gMaterial.color * textureColor * gDirectionalLight.color * cos * gDirectionalLight.intensity;
        */
        //output.color.rgb = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.intensity;
        //output.color.a = gMaterial.color.a * textureColor.a;
    } else { // Lightingしない場合。
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}