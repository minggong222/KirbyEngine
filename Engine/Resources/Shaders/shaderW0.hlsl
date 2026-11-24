// ShaderW0.hlsl

// ShaderW0.hlsl 소스 파일 맨위에 아래 상수버퍼 선언을 추가 하세요.
cbuffer constants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 View;
    row_major float4x4 Projection;

    uint IsOutline;
    uint IsTextured;
    float2 ConstantsPadding;
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
};

PS_INPUT mainVS(VS_INPUT input)
{
    PS_INPUT output;
    
    // Pass the position directly to the pixel shader (no transformation)
    //output.position = input.position;
    
    // 상수버퍼를 통해 넘겨 받은 Offset을 더해서 버텍스를 이동 시켜 픽셀쉐이더로 넘김
    float4 WorldPosition = mul(float4(input.position.xyz, 1.0f), World);
    float4 ViewPosition = mul(WorldPosition, View);
    output.position = mul(ViewPosition, Projection);
    
    // Pass the color to the pixel shader
    output.color = IsOutline != 0 ? float4(1.0f, 0.45f, 0.05f, 1.0f) : input.color;
    output.uv = input.uv;
    
    return output;
}

float4 mainPS(PS_INPUT input) : SV_TARGET
{
    if (IsTextured != 0)
    {
        const float4 TextureColor = BaseColorTexture.Sample(BaseColorSampler, input.uv);
        return TextureColor * input.color;
    }

    return input.color;
}