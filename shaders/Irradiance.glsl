//------------------------------------------------------------------------------

-- Compute

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;
layout(rgba16f, binding=0) uniform writeonly imageCube uCube;

const uint sampleCount = 32u;

shared float fInvTotalWeight;
shared vec3 vSampleDirections[sampleCount];
shared float fSampleMipLevels[sampleCount];
shared float fSampleWeights[sampleCount];

uniform samplerCube uEnvMap;

const float pi = 3.14159265359;

// ----------------------------------------------------------------------------
// http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
// efficient VanDerCorpus calculation.
float RadicalInverse_VdC(uint bits) 
{
     bits = (bits << 16u) | (bits >> 16u);
     bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
     bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
     bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
     bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
     return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
// ----------------------------------------------------------------------------
vec2 Hammersley(uint i, uint N)
{
	return vec2(float(i)/float(N), RadicalInverse_VdC(i));
}
// ----------------------------------------------------------------------------
vec4 ImportanceSampleHemisphereUniform(vec2 Xi, vec3 N)
{
	float phi = 2 * pi * Xi.x;
	float cosTheta = Xi.y;
	float sinTheta = sqrt( 1 - cosTheta * cosTheta );

	vec3 H;
	H.x = sinTheta * cos(phi);
	H.y = sinTheta * sin(phi);
	H.z = cosTheta;
	
	float pdf = 1.0 / (2 * pi);

	vec3 Up = abs(N.z) < 0.999 ? vec3(0,0,1) : vec3(1,0,0);
	vec3 T = normalize( cross( Up, N ) );
	vec3 B = cross( N, T );
			 
	return vec4((T * H.x) + (B * H.y) + (N * H.z), pdf); 
}

#if 0
float CalcMipLevel(vec3 H, float Roughness)
{
    // sample from the environment's mip level based on roughness/pdf
    float D = DistributionGGX(vec3(0, 0, 1), H, Roughness);
    float ndoth = max(H.z, 0.0);
    float hdotv = max(H.z, 0.0);
    float pdf = D * ndoth / (4.0 * hdotv) + 0.0001;
    float resolution = 512.0; // resolution of source cubemap (per face)
    // Solid angle covered by 1 pixel with 6 faces that are resolution x resolution
    float omegaP = 4.0 * pi / (6.0 * resolution * resolution);
    // Solid angle represented by this sample
    float omegaS = 1.0 / (float(sampleCount) * pdf + 0.0001);
    // Original paper suggest biasing the mip to improve the results	
    float mipBias = 1.0;
    float mipLevel = 0.0;
    // Remove dot pattern at roughness 0
    if(Roughness > 0.0)
        mipLevel = max(0.5 * log2(omegaS / omegaP) + mipBias, 0.0);
    return mipLevel;
}
#endif

vec3 DiffuseIBL(vec3 N)
{
	// from tangent-space H vector to world-space sample vector
	// vec3 up        = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
	// vec3 tangent   = normalize(cross(up, N));
	// vec3 bitangent = cross(N, tangent);
    // mat3 tangentToWorld = mat3(tangent, bitangent, N);
	
	vec3 color = vec3(0.0);
    float weight = 0.0;
	for (uint s = 0u; s < sampleCount; s++)
	{
        vec2 Xi = Hammersley(s, sampleCount);
		vec4 Sample = ImportanceSampleHemisphereUniform(Xi, N);
        vec3 L = Sample.xyz; 
        float pdf = Sample.w;
        float ndotl = clamp(dot(L, N), 0, 1);
        if (ndotl > 0)
        {
            color += textureLod(uEnvMap, L, 0).rgb * ndotl;
            weight += ndotl;
        }
	}
	return color / weight;
}

// Use code glow-extras's
vec3 Direction(float x, float y, uint l)
{
	// see ogl spec 8.13. CUBE MAP TEXTURE SELECTION	
	switch(l) {
		case 0: return vec3(+1, -y, -x); // +x
		case 1: return vec3(-1, -y, +x); // -x
		case 2: return vec3(+x, +1, +y); // +y
		case 3: return vec3(+x, -1, -y); // -y
		case 4: return vec3(+x, -y, +1); // +z
		case 5: return vec3(-x, -y, -1); // -z
	}
	return vec3(0, 1, 0);
}

void main()
{
#if 0
    uint si = gl_LocalInvocationIndex;
    if (si < sampleCount)
    {
        vec2 Xi = Hammersley(si, sampleCount);
        vec3 H = ImportanceSampleGGX(Xi, uRoughness);
    	vec3 V = vec3(0, 0, 1);
	
        // Optimized local coordinate ref. placeholderart [7]
        float mipLevel = CalcMipLevel(H, uRoughness);

        // Compute local reflected vector L from H
        vec3 L = normalize(2.0 * H.z * H - V);
        fSampleMipLevels[si] = mipLevel;
        vSampleDirections[si] = L;
        fSampleWeights[si] = L.z;
    }
    // Ensure shared memory writes are visible to work group
    memoryBarrierShared();

    // Ensure all threads in work group   
    // have executed statements above
    barrier();
#endif

	uint x = gl_GlobalInvocationID.x;	
	uint y = gl_GlobalInvocationID.y;
	uint l = gl_GlobalInvocationID.z;
	ivec2 s = imageSize(uCube);

	// check out of bounds
	if (x >= s.x || y >= s.y)
		return;

	float fx = (float(x) + 0.5) / float(s.x);
	float fy = (float(y) + 0.5) / float(s.y);

	vec3 dir = normalize(Direction(fx * 2 - 1, fy * 2 - 1, l));	
	vec3 color = DiffuseIBL(dir);

	imageStore(uCube, ivec3(x, y, l), vec4(color, 0));
}
