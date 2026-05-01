#define COLOR_MATRIX_BT601_LIMITED  0
#define COLOR_MATRIX_BT601_FULL     1
#define COLOR_MATRIX_BT709_LIMITED  2
#define COLOR_MATRIX_BT709_FULL     3
#define COLOR_MATRIX_BT2020_LIMITED 4
#define COLOR_MATRIX_COUNT          5

// 常量缓冲区只需要一个索引
cbuffer ColorConvParams : register(b0)
{
    uint matrixIndex;
};

// 预先定义转换矩阵 (每个矩阵是3个float4)
static const float4 g_matrices[COLOR_MATRIX_COUNT][3] = {
    // BT.601 Limited -> Full Range RGB
    {
        float4(1.1644,  0.0000,  1.5960, -0.8708),
        float4(1.1644, -0.3918, -0.8130,  0.5296),
        float4(1.1644,  2.0172,  0.0000, -1.0813)
    },
    // BT.601 Full -> Full Range RGB
    {
        float4(1.0000,  0.0000,  1.4020, -0.7010),
        float4(1.0000, -0.3441, -0.7141,  0.5291),
        float4(1.0000,  1.7720,  0.0000, -0.8860)
    },
    // BT.709 Limited -> Full Range RGB
    {
        float4(1.1644,  0.0000,  1.7927, -0.9729),
        float4(1.1644, -0.2132, -0.5329,  0.3015),
        float4(1.1644,  2.1124,  0.0000, -1.1335)
    },
    // BT.709 Full -> Full Range RGB
    {
        float4(1.0000,  0.0000,  1.5748, -0.7874),
        float4(1.0000, -0.1873, -0.4681,  0.3277),
        float4(1.0000,  1.8556,  0.0000, -0.9278)
    },
    // BT.2020 Limited -> Full Range RGB (示例)
    {
        float4(1.1644,  0.0000,  1.6787, -0.9157),
        float4(1.1644, -0.1873, -0.6504,  0.3475),
        float4(1.1644,  2.1418,  0.0000, -1.1483)
    }
};

struct VertexOutPut {
    float4 pos: SV_POSITION;
    float2 uv: TEXCOORD;
};

Texture2D<float> texY : register(t0);
Texture2D<float2> texUV : register(t1);
SamplerState samplerState : register(s0);

float4 main(VertexOutPut vertex) : SV_TARGET
{
    // 1. 采样 NV12 (与之前相同)
    float Y = texY.Sample(samplerState, vertex.uv).r;
    float2 UV = texUV.Sample(samplerState, vertex.uv).rg;
    //float2 UV = float2(0.5, 0.5);
    // 2. 如果矩阵为 Limited 类型，已在矩阵中包含了范围映射，无需额外步骤
    //    (我们上面定义的矩阵已经将 16-235 YUV 映射到了 0-1 RGB)
    //    若使用 Full 矩阵，则采样值本身已是 0-1 范围。

    float4 yuv = float4(Y, UV.x, UV.y, 1.0);

    // 3. 选择矩阵并计算 RGB
    //uint idx = clamp(matrixIndex, 0, COLOR_MATRIX_COUNT - 1);
    uint idx = 3;
    float4 rgba;
    rgba.r = dot(g_matrices[idx][0], yuv);
    rgba.g = dot(g_matrices[idx][1], yuv);
    rgba.b = dot(g_matrices[idx][2], yuv);
    rgba.a = 1.0;

    return rgba;
}