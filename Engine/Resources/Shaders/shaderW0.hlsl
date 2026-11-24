cbuffer constants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    uint IsOutline;
    uint IsTextured;
    uint IsUnlit;
    uint ConstantsPadding;
}

Texture2D BaseColorTexture : register(t0);
SamplerState BaseColorSampler : register(s0);

struct VS_INPUT
{
    float4 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    const float4 WorldPosition = mul(float4(input.position.xyz, 1.0f), World);
    const float4 ViewPosition = mul(WorldPosition, View);
    output.position = mul(ViewPosition, Projection);
    output.color = IsOutline != 0 ? float4(1.0f, 0.45f, 0.05f, 1.0f) : input.color;
    output.uv = input.uv;
    output.worldPosition = WorldPosition.xyz;
    return output;
}

float4 mainPS(PS_INPUT input, bool bFrontFace : SV_IsFrontFace) : SV_TARGET
{
    float4 BaseColor = IsTextured != 0 ? BaseColorTexture.Sample(BaseColorSampler, input.uv) * input.color : input.color;
    if (IsUnlit != 0 || IsOutline != 0)
    {
        return BaseColor;
    }

    float3 Normal = normalize(cross(ddx(input.worldPosition), ddy(input.worldPosition)));
    if (!bFrontFace) Normal = -Normal;
    const float3 LightDirection = normalize(float3(-0.35f, -0.45f, 0.82f));
    const float Diffuse = saturate(dot(Normal, LightDirection));
    const float Lighting = 0.22f + Diffuse * 0.78f;
    BaseColor.rgb *= Lighting;
    return BaseColor;
}
