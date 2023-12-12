#include "Common.hlsl"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 NormalW : NORMAL;
};


float4 main(VertexOut pin) : SV_Target
{
    pin.NormalW = normalize(pin.NormalW);
    float3 normalV = mul(pin.NormalW, (float3x3) view);
    return float4(normalV, 0.0f);
}