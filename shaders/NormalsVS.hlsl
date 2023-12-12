#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float3 TangentU : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
};


VertexOut main(VertexIn vin)
{
    VertexOut vout = (VertexOut) 0.0f;
    vout.NormalW = mul(vin.NormalL, (float3x3) world);

    // Transform to homogeneous clip space.
    float4 posW = mul(float4(vin.PosL, 1.0f), world);
    vout.PosH = mul(posW, viewProj);

    return vout;
}