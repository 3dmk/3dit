// CoreModel Basic HLSL Shader v0.1
// Target: Direct3D 11, Shader Model 5.0
// Entry points: VSMain (vs_5_0), PSMain (ps_5_0)
// Coordinate convention: Z-up world; matrices supplied by the D3D backend.
// This file is not consumed by the existing raylib/OpenGL viewport.
cbuffer SceneConstants : register(b0)
{
    row_major float4x4 World;
    row_major float4x4 ViewProjection;
    float4 LightDirection; // xyz: direction light travels, normalized
    float4 LightColor;     // rgb: light radiance, a unused
    float4 AmbientColor;   // rgb: ambient contribution, a unused
    float4 BaseColor;      // rgba: material color
};

struct VSInput
{
    float3 Position : POSITION;
    float3 Normal   : NORMAL;
};

struct VSOutput
{
    float4 Position : SV_POSITION;
    float3 NormalWS : TEXCOORD0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    float4 positionWS = mul(float4(input.Position, 1.0f), World);
    output.Position = mul(positionWS, ViewProjection);
    // For nonuniform scaling, the backend should supply an inverse-transpose
    // normal matrix. This basic shader assumes uniform scale.
    output.NormalWS = normalize(mul(float4(input.Normal, 0.0f), World).xyz);
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float3 N = normalize(input.NormalWS);
    float3 L = normalize(-LightDirection.xyz);
    float diffuse = saturate(dot(N, L));
    float3 lighting = AmbientColor.rgb + LightColor.rgb * diffuse;
    float3 color = saturate(BaseColor.rgb * lighting);
    return float4(color, BaseColor.a);
}
