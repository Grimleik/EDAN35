#include "Common.hlsl"

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float2 TexC : TEXCOORD;
};

float4 main(VertexOut pin) : SV_Target
{
    //return float4(1.0f, 0.0f, 0.0f, 1.0f);
    return float4(ssaoTex.Sample(linearSamp, pin.TexC).rrr, 1.0f);
}


