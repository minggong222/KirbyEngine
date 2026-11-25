cbuffer constants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;
    row_major float4x4 NormalMatrix;
    float4 MaterialColor;
    float2 UVOffset;
    uint2 MaterialPadding;
    uint IsOutline;
    uint IsTextured;
    uint IsUnlit;
    uint UseVertexNormals;
}

Texture2D BaseColorTexture : register(t0);
SamplerState BaseColorSampler : register(s0);

struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 normal : NORMAL;
};

struct PS_INPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 worldPosition : TEXCOORD1;
    float3 worldNormal : TEXCOORD2;
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;

    const float4 WorldPosition = mul(float4(input.position, 1.0f), World);
    const float4 ViewPosition = mul(WorldPosition, View);

    output.position = mul(ViewPosition, Projection);
    output.color = IsOutline != 0 ? float4(1.0f, 0.45f, 0.05f, 1.0f) : input.color;
    output.uv = input.uv + UVOffset;
    output.worldPosition = WorldPosition.xyz;
    output.worldNormal = normalize(mul(float4(input.normal, 0.0f), NormalMatrix).xyz);

    return output;
}

float4 mainPS(PS_INPUT input, bool bFrontFace : SV_IsFrontFace) : SV_TARGET
{
    float4 BaseColor = input.color * MaterialColor;

    if (IsTextured != 0)
    {
        BaseColor *= BaseColorTexture.Sample(BaseColorSampler, input.uv);
    }

    if (IsUnlit != 0 || IsOutline != 0)
    {
        return BaseColor;
    }

    float3 Normal;

    if (UseVertexNormals != 0)
    {
        Normal = normalize(input.worldNormal);
    }
    else
    {
        Normal = normalize(cross(ddx(input.worldPosition), ddy(input.worldPosition)));
    }

    if (!bFrontFace)
    {
        Normal = -Normal;
    }

    const float3 LightDirection = normalize(float3(-0.35f, -0.45f, 0.82f));
    const float Diffuse = saturate(dot(Normal, LightDirection));
    const float Lighting = 0.22f + Diffuse * 0.78f;

    BaseColor.rgb *= Lighting;

    return BaseColor;
}