struct VertexShaderOutput {
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
    float3 normal : NORMAL0;
    float3 worldPosition : POSITION0;
};

struct Camera {
    float3 worldPosition;
    float pad;
};

struct PointLight {
    float4 color; //!< ライトの色
    float3 position; //!< ライトの位置
    float intensity; //!< 輝度
    float radius; //!< ライトの届く最大距離
    float decay; //!< 減衰率
    float padding[2];
};

struct SpotLight {
    float4 color; //!< ライトの色
    float3 position; //!< ライトの位置
    float intensity; //!< 輝度
    float3 direction; //!< スポットライトの方向
    float distance; //!< ライトの届く最大距離
    float decay; //!< 減衰率
    float cosAngle; //!< スポットライトの余弦
    float cosFalloffStart;
};