#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
};


VertexOut main(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.NormalW = mul(vin.NormalL, (float3x3) world);
    vout.TangentW = mul(vin.TangentU, (float3x3) world);

    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosH = mul(posW, viewProj);

    return vout;
}