#include "Fullscreen.hlsli"

// 輝度ベースのアウトライン（資料CG5 00_06）。
// シーン色を輝度(Luminance)へ変換し、Prewittフィルタで輝度差の大きい場所＝エッジを検出する。
// 深度版(DepthBasedOutline)と違い入力はシーン色のみ。形の境界に加えテクスチャの模様にも線が出る
Texture2D<float4> gTexture : register(t0); // 元画像(シーン色)
SamplerState gSampler : register(s0);

struct OutlineParam {
    float3 lineColor; // 線の色(既定は黒)
    float strength;   // 差分の増幅率(資料の「適当に6倍」をCBufferで調整可能にしたもの)
    int debugView;    // 1でエッジ(weight)をそのまま白黒表示
    float3 _pad;
};
ConstantBuffer<OutlineParam> gParam : register(b0);

struct PixelShaderOutput {
    float4 color : SV_TARGET0;
};

// 横方向Prewitt
static const float kPrewittH[3][3] = {
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
    { -1.0f / 6.0f, 0.0f, 1.0f / 6.0f },
};
// 縦方向Prewitt
static const float kPrewittV[3][3] = {
    { -1.0f / 6.0f, -1.0f / 6.0f, -1.0f / 6.0f },
    { 0.0f, 0.0f, 0.0f },
    { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f },
};

// RGB→輝度。Grayscale(Monochrome)と同じITU-R BT.709の係数
float Luminance(float3 v) {
    return dot(v, float3(0.2125f, 0.7154f, 0.0721f));
}

PixelShaderOutput main(VertexShaderOutput input) {
    PixelShaderOutput output;

    uint width, height;
    gTexture.GetDimensions(width, height); // texelサイズは既存流儀でここから
    float2 uvStep = float2(rcp((float) width), rcp((float) height));

    // 縦横それぞれの畳み込みの結果を格納する
    float2 difference = float2(0.0f, 0.0f);

    [unroll]
    for (int x = 0; x < 3; ++x) {
        [unroll]
        for (int y = 0; y < 3; ++y) {
            float2 uv = input.texcoord + float2(x - 1, y - 1) * uvStep;
            // 色を輝度に変換してから畳み込む(深度版が深度を読む箇所の置き換え)
            float luminance = Luminance(gTexture.Sample(gSampler, uv).rgb);
            difference.x += luminance * kPrewittH[x][y];
            difference.y += luminance * kPrewittV[x][y];
        }
    }

    // 差分が大きいほどエッジらしい。輝度差はそのままだと小さいのでstrengthで増幅する
    float weight = saturate(length(difference) * gParam.strength);

    if (gParam.debugView != 0) { // 調整用：白いほどエッジ
        output.color = float4(weight.xxx, 1.0f);
        return output;
    }

    float3 sceneColor = gTexture.Sample(gSampler, input.texcoord).rgb;
    // 線色へ寄せる合成。lineColor=黒なら資料の (1-weight)*色 と同じ結果になる
    output.color.rgb = lerp(sceneColor, gParam.lineColor, weight);
    output.color.a = 1.0f;
    return output;
}
