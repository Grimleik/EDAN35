#include "CommonSSAO.hlsl"

float OcclusionFunction(float distZ)
{
    float occlusion = 0.0f;
    if (distZ > gSurfaceEpsilon)
    {
        float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
        occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
    }
	
    return occlusion;
}

float NdcDepthToViewDepth(float z_ndc)
{
    // z_ndc = A + B/viewZ, where gProj[2,2]=A and gProj[3,2]=B.
    float viewZ = gProj[3][2] / (z_ndc - gProj[2][2]);
    return viewZ;
}
 
float4 main(VertexOut pin) : SV_Target
{
    float3 n = normalize(gNormalMap.SampleLevel(gsamPointClamp, pin.TexC, 0.0f).xyz);
    float pz = gDepthMap.SampleLevel(gsamDepthMap, pin.TexC, 0.0f).r;
    return float4(pz, 0, 0, 1.0);

    pz = NdcDepthToViewDepth(pz);
    float3 p = (pz / pin.PosV.z) * pin.PosV;
	
	// Extract random vector and map from [0,1] --> [-1, +1].
    float3 randVec = 2.0f * gRandomVecMap.SampleLevel(gsamLinearWrap, 4.0f * pin.TexC, 0.0f).rgb - 1.0f;

    float occlusionSum = 0.0f;
	
	// Sample neighboring points about p in the hemisphere oriented by n.
    for (int i = 0; i < gSampleCount; ++i)
    {
        float3 offset = reflect(gOffsetVectors[i].xyz, randVec);
	
		// Flip offset vector if it is behind the plane defined by (p, n).
        float flip = sign(dot(offset, n));
		
		// Sample a point near p within the occlusion radius.
        float3 q = p + flip * gOcclusionRadius * offset;
		
		// Project q and generate projective tex-coords.  
        float4 projQ = mul(float4(q, 1.0f), gProjTex);
        projQ /= projQ.w;
        float rz = gDepthMap.SampleLevel(gsamDepthMap, projQ.xy, 0.0f).r;
        rz = NdcDepthToViewDepth(rz);
        float3 r = (rz / q.z) * q;
        float distZ = p.z - r.z;
        float dp = max(dot(n, normalize(r - p)), 0.0f);
        
        float occlusion = 0.0f;
        if (distZ > gSurfaceEpsilon)
        {
            float fadeLength = gOcclusionFadeEnd - gOcclusionFadeStart;
            occlusion = saturate((gOcclusionFadeEnd - distZ) / fadeLength);
        }
        
        occlusionSum += dp * occlusion;
    }
	
    occlusionSum /= gSampleCount;
	
    float access = 1.0f - occlusionSum;
    return access;
}
