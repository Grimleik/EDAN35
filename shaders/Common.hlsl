cbuffer cb : register(b0)
{
    float4x4 world;
    float4x4 view;
    float4x4 viewProj;
};

Texture2D ssaoTex : register(t0);
SamplerState linearSamp : register(s0);

