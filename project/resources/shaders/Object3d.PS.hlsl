#include "Object3d.hlsli"
struct Material {
    float4 color;
    int enableLighting;
    int useEnvironmentMap;
    float dissolve;
    float4x4 uvTransform;
    float shininess;
};
ConstantBuffer<Material> gMaterial : register(b0);
struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};
Texture2D<float4> gTexture : register(t0);
TextureCube<float4> gEnvironmentTexture : register(t1);
SamplerState gSampler : register(s0);

struct DirectionalLight {
    float4 color; //!< ライトの色
    float3 direction; //!< ライトの向き
    float intensity; //!< 輝度
};

ConstantBuffer<DirectionalLight> gDirectionalLight : register(b1);
ConstantBuffer<Camera> gCamera : register(b2);
ConstantBuffer<PointLight> gPointLight : register(b3);
ConstantBuffer<SpotLight> gSpotLight : register(b4);

PixelShaderOutput main(VertexShaderOutput input) {
    
    // 4x4 Bayerマトリクス（市松模様の細かいバージョン）
    static const float bayer[4][4] = {
        { 0.0 / 16.0, 8.0 / 16.0, 2.0 / 16.0, 10.0 / 16.0 },
        { 12.0 / 16.0, 4.0 / 16.0, 14.0 / 16.0, 6.0 / 16.0 },
        { 3.0 / 16.0, 11.0 / 16.0, 1.0 / 16.0, 9.0 / 16.0 },
        { 15.0 / 16.0, 7.0 / 16.0, 13.0 / 16.0, 5.0 / 16.0 }
    };

    // 画面座標からどのマス目にいるか計算
    uint px = (uint) input.position.x % 4;
    uint py = (uint) input.position.y % 4;

    // dissolveがマス目の閾値より大きければそのピクセルを消す
    if (gMaterial.dissolve > bayer[py][px]) {
        discard;
    }
    
    PixelShaderOutput output;
    
    float4 transformedUV = mul(float4(input.texcoord, 0.0f, 1.0f), gMaterial.uvTransform);
    float4 textureColor = gTexture.Sample(gSampler, transformedUV.xy);
    
    if (textureColor.a <= 0.01f) {
        discard;
    }
    
    if (gMaterial.enableLighting != 0) { // Lightingする場合
        
        float3 toEye = normalize(gCamera.worldPosition - input.worldPosition);
        
        // Directional
        float NDotL_D = dot(normalize(input.normal), -gDirectionalLight.direction);
        float cosD = pow(NDotL_D * 0.5f + 0.5f, 2.0f);
        float3 halfVectorD = normalize(-gDirectionalLight.direction + toEye);
        float NDotH_D = dot(normalize(input.normal), halfVectorD);
        float specularPowD = pow(saturate(NDotH_D), gMaterial.shininess);
        // 拡散反射
        float3 diffuseDirectionalLight = gMaterial.color.rgb * textureColor.rgb * gDirectionalLight.color.rgb * gDirectionalLight.intensity * cosD;
        // 鏡面反射
        float3 specularDirectionalLight = gDirectionalLight.color.rgb * gDirectionalLight.intensity * specularPowD * float3(1.0f, 1.0f, 1.0f);
        
        // Point
        float distance = length(gPointLight.position - input.worldPosition); // ポイントライトへの距離
        float factor = pow(saturate(-distance / gPointLight.radius + 1.0), gPointLight.decay); // 指数によるコントロール
        float3 pointLightDirection = normalize(gPointLight.position - input.worldPosition);
        float NdotL_P = dot(normalize(input.normal), pointLightDirection);
        float cosP = pow(NdotL_P * 0.5f + 0.5f, 2.0f);
        float3 halfVectorP = normalize(pointLightDirection + toEye);
        float NDotH_P = dot(normalize(input.normal), halfVectorP);
        float specularPowP = pow(saturate(NDotH_P), gMaterial.shininess);
        float3 pointLightColor = gPointLight.color.rgb * gPointLight.intensity * factor;
        // 拡散反射
        float3 diffusePointLight = gMaterial.color.rgb * textureColor.rgb * pointLightColor * cosP;
        // 鏡面反射
        float3 specularPointLight = pointLightColor * specularPowP * float3(1.0f, 1.0f, 1.0f);
        
        // Spot
        float spotDistance = length(gSpotLight.position - input.worldPosition);
        float spotFactor = pow(saturate(-spotDistance / gSpotLight.distance + 1.0), gSpotLight.decay);
        float cosAngleCurrent = dot(normalize(input.worldPosition - gSpotLight.position), normalize(gSpotLight.direction));
        float falloffFactor = saturate((cosAngleCurrent - gSpotLight.cosAngle) / (gSpotLight.cosFalloffStart - gSpotLight.cosAngle));
        float3 spotLightDirectionOnSurface = normalize(gSpotLight.position - input.worldPosition);
        float NDotL_S = dot(normalize(input.normal), spotLightDirectionOnSurface);
        float cosS = pow(NDotL_S * 0.5f + 0.5f, 2.0f);
        float3 halfVectorS = normalize(spotLightDirectionOnSurface + toEye);
        float NDotH_S = dot(normalize(input.normal), halfVectorS);
        float specularPowS = pow(saturate(NDotH_S), gMaterial.shininess);
        float3 spotLightColor = gSpotLight.color.rgb * gSpotLight.intensity * spotFactor * falloffFactor;
        float3 diffuseSpotLight = gMaterial.color.rgb * textureColor.rgb * spotLightColor * cosS;
        float3 specularSpotLight = spotLightColor * specularPowS * float3(1.0f, 1.0f, 1.0f);
        
        // 拡散反射 + 鏡面反射
        output.color.rgb = diffuseDirectionalLight + specularDirectionalLight + diffusePointLight + specularPointLight + diffuseSpotLight + specularSpotLight;

        // 環境マップ
        if (gMaterial.useEnvironmentMap != 0) {
            float3 cameraToPosition = normalize(input.worldPosition - gCamera.worldPosition);
            float3 reflectedVector = reflect(cameraToPosition, normalize(input.normal));
            float4 environmentColor = gEnvironmentTexture.Sample(gSampler, reflectedVector);
            //output.color.rgb += environmentColor.rgb;
            output.color.rgb += environmentColor.rgb * gMaterial.color.a;
        }
        
        output.color.a = gMaterial.color.a * textureColor.a;
        
    } else { // Lightingしない場合。
        output.color = gMaterial.color * textureColor;
    }
    
    return output;
}