//--------------------------------------------------------------
// Copyright 2026, Ed Keenan, all rights reserved.
//--------------------------------------------------------------

// Enable data
#define SRV_tKeyAa
#define SRV_tKeyAb
#define UAV_uMixerOutAx
#define CBV_csMixerA
        
#include "ShaderMappings.hlsli"

// ---------------------------------------------
// Functions
// ---------------------------------------------
float4 slerp(float4 src, float4 tar, float t);
float4 lerpMe(float4 src, float4 tar, float t);

// ---------------------------------------------
// Local Threads
// ---------------------------------------------
[numthreads(1, 1, 1)]

// ---------------------------------------------
// main
// ---------------------------------------------
void main(uint3 dtID : SV_DispatchThreadID)
{
    uint boneIndex = dtID.x;

    // protection incase index is outside array range
    if (dtID.x < csMixerA.numNodes)
    {
        uMixerOutAx[boneIndex] = tKeyAa[boneIndex];
  
        float4 trans = lerpMe(tKeyAa[boneIndex].t, tKeyAb[boneIndex].t, csMixerA.ts);
        float4 quat = slerp(tKeyAa[boneIndex].q, tKeyAb[boneIndex].q, csMixerA.ts);
        float4 scale = lerpMe(tKeyAa[boneIndex].s, tKeyAb[boneIndex].s, csMixerA.ts);

        uMixerOutAx[boneIndex].t = trans;
        uMixerOutAx[boneIndex].q = quat;
        uMixerOutAx[boneIndex].s = scale;
    }
	
}

float4 lerpMe(float4 src, float4 tar, float t)
{
    float4 result;
    
    result.x = src.x + t * (tar.x - src.x);
    result.y = src.y + t * (tar.y - src.y);
    result.z = src.z + t * (tar.z - src.z);
    result.w = 1.0f;
    
    return result;
}

float4 slerp(float4 src, float4 tar, float t)
{
    float4 result;
    float4 target;
    float QUAT_EPSILON = 0.001f;

	// Not sure if dot is 3D or 4D... need 4D
	//float cosom = dot(src, tar);
    float cosom = src.x * tar.x + src.y * tar.y + src.z * tar.z + src.w * tar.w;

    if (cosom >= 1.0f)
    {
		// do nothing
        result = src;
    }
    else
    {
		// adjust signs if necessary
        if (cosom < 0.0f)
        {
            cosom = -cosom;
            target = -tar;
        }
        else
        {
            target = tar;
        }

		// If the the source and target are close, we can do a lerp.
        float tarFactor = t;
        float srcFactor = 1.0f - t;

		// calculate coefficients
        if (cosom < (1.0f - QUAT_EPSILON))
        {
			// Quats are not close enough for a lerp.
			// Calculating coefficients for a slerp.
            const float omega = acos(cosom);
            const float sinom = 1.0f / sin(omega);

            srcFactor = sin(srcFactor * omega) * sinom;
            tarFactor = sin(tarFactor * omega) * sinom;
        }

        result.x = src.x * srcFactor + target.x * tarFactor;
        result.y = src.y * srcFactor + target.y * tarFactor;
        result.z = src.z * srcFactor + target.z * tarFactor;
        result.w = src.w * srcFactor + target.w * tarFactor;
    }

    return result;
}


// --- End of File ---
