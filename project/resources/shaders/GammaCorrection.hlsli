#ifndef GAMMA_CORRECTION_HLSLI
#define GAMMA_CORRECTION_HLSLI

// IEC 61966-2-1 準拠の正確なsRGB変換
float3 LinearToSRGB(float3 linearColor)
{
    float3 sRGBLo = linearColor * 12.92;
    float3 sRGBHi = pow(abs(linearColor), float3(1.0 / 2.4, 1.0 / 2.4, 1.0 / 2.4))
                    * 1.055 - 0.055;
    // HLSL には GLSL の select が存在しないため、mask を作って lerp を使う
    float3 threshold = float3(0.0031308, 0.0031308, 0.0031308);
    float3 mask = step(linearColor, threshold); // mask == 1 なら sRGBLo を選ぶ
    float3 sRGB = lerp(sRGBHi, sRGBLo, mask);
    return sRGB;
}

// シンプル版 (pow 2.2)
float3 LinearToGamma(float3 linearColor)
{
    return pow(abs(linearColor), float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));
}

#endif // GAMMA_CORRECTION_HLSLI