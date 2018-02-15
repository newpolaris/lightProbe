-- Vertex

// IN
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoords;

// OUT
out vec3 vNormalWS;
out vec3 vViewDirWS;
out vec3 vWorldPosWS;
out vec2 vTexcoords;

// UNIFORM
uniform mat4 uMtxSrt;
uniform mat4 uModelViewProjMatrix;
uniform vec3 uEyePosWS;

void main()
{
  // Clip Space position
  gl_Position = uModelViewProjMatrix * uMtxSrt * inPosition;

  // World Space normal
  vec3 normal = mat3(uMtxSrt) * inNormal;
  vNormalWS = normalize(normal);

  vTexcoords = inTexcoords;
  
  // World Space view direction from world space position
  vec3 posWS = vec3((uMtxSrt * inPosition).xyz);
  vViewDirWS = normalize(uEyePosWS - posWS);
  vWorldPosWS = posWS;
}


--

//------------------------------------------------------------------------------


-- Fragment

#include "ToneMappingUtility.glsli"

// IN
in vec3 vNormalWS;
in vec3 vViewDirWS;
in vec3 vWorldPosWS;
in vec2 vTexcoords;

// OUT
layout(location = 0) out vec4 fragColor;

// UNIFORM
uniform samplerCube uEnvmapIrr;
uniform samplerCube uEnvmapPrefilter;
uniform sampler2D uEnvmapBrdfLUT;

uniform float ubMetalOrSpec;
uniform float ubDiffuse;
uniform float ubSpecular;
uniform float ubDiffuseIbl;
uniform float ubSpecularIbl;
uniform float uGlossiness;
uniform float uReflectivity;
uniform float uExposure;
uniform vec3 uLightDir;
uniform vec3 uLightCol;
uniform vec3 uRgbDiff;
uniform vec3 uLightPositions[4];
uniform vec3 uLightColors[4];

const float pi = 3.14159265359;

vec3 calcFresnel(vec3 _cspec, float _dot, float _strength)
{
	return _cspec + (1.0 - _cspec)*pow(1.0 - _dot, 5.0) * _strength;
}

vec3 calcLambert(vec3 _cdiff, float _ndotl)
{
	return _cdiff*_ndotl;
}

vec3 calcBlinn(vec3 _cspec, float _ndoth, float _ndotl, float _specPwr)
{
	float norm = (_specPwr+8.0)*0.125;
	float brdf = pow(_ndoth, _specPwr)*_ndotl*norm;
	return _cspec*brdf;
}

float specPwr(float _gloss)
{
	return exp2(10.0*_gloss+2.0);
}

float random(vec2 _uv)
{
  return fract(sin(dot(_uv.xy, vec2(12.9898, 78.233) ) ) * 43758.5453);
}

vec3 fixCubeLookup(vec3 _v, float _lod, float _topLevelCubeSize)
{
  // Reference:
  // Seamless cube-map filtering
  // http://the-witness.net/news/2012/02/seamless-cube-map-filtering/
  float ax = abs(_v.x);
  float ay = abs(_v.y);
  float az = abs(_v.z);
  float vmax = max(max(ax, ay), az);
  float scale = 1.0 - exp2(_lod) / _topLevelCubeSize;
  if (ax != vmax) { _v.x *= scale; }
  if (ay != vmax) { _v.y *= scale; }
  if (az != vmax) { _v.z *= scale; }
  return _v;
}

// use code from learnOpenGL
float distributionGGX(float _ndoth, float roughness)
{
	// due to alpha = roughness^2
	float a = roughness*roughness;
	float a2 = a*a;
	float denom = _ndoth*_ndoth * (a2 - 1.0) + 1.0;
	denom = max(0.001, denom);
	return a2 / (pi * denom * denom);
}

float geometrySchlickGGX(float _ndotv, float roughness)
{
	float r = roughness + 1.0;
	float k = r*r / 8.0;
	float nom = _ndotv;
	float denom = _ndotv * (1.0 - k) + k;
	return nom / denom;
}

float geometrySmith(float _ndotv, float _ndotl, float roughness)
{
	float ggx2 = geometrySchlickGGX(_ndotv, roughness);
	float ggx1 = geometrySchlickGGX(_ndotl, roughness);
	return ggx1 * ggx2;
}

mat3 calcTbn(vec3 _normal, vec3 _worldPos, vec2 _texCoords)
{
    vec3 Q1  = dFdx(_worldPos);
    vec3 Q2  = dFdy(_worldPos);
    vec2 st1 = dFdx(_texCoords);
    vec2 st2 = dFdy(_texCoords);

    vec3 N   = _normal;
    vec3 T  = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B  = -normalize(cross(N, T));
    return mat3(T, B, N);
}

// Moving frostbite to pbr
vec3 getSpecularDomninantDir(vec3 N, vec3 R, float roughness)
{
    float smoothness = 1 - roughness;
    float lerpFactor = smoothness * (sqrt(smoothness) + roughness);
    // The result is not normalized as we fetch in a cubemap
    return mix(N, R, lerpFactor);
}

void main()
{  
  // Material params. uMetallicMap
  vec3  inAlbedo = uRgbDiff;
  float inMetallic = uReflectivity;
  float inRoughness = uGlossiness;

  // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
  // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)    
  vec3 f0 = mix(vec3(0.04), inAlbedo, inMetallic);

  // multiply kD by the inverse metalness such that only non-metals 
  // have diffuse lighting, or a linear blend if partly metal (pure metals
  // have no diffuse light).
  vec3 albedo = inAlbedo * (1.0 - inMetallic);

  // Input.
  vec3 nn = normalize(vNormalWS);
  vec3 vv = normalize(vViewDirWS);

  // reflectance equation
  vec3 direct = vec3(0.0);
  for (int i = 0; i < 4; ++i)
  {
	  // calculate per-light radiance
	  vec3 ld = normalize(uLightPositions[i] - vWorldPosWS);
	  vec3 hh = normalize(vv + ld);
	  float distance = length(uLightPositions[i] - vWorldPosWS);
	  float attenuation = 1.0 / (distance*distance);
	  vec3 radiance = uLightColors[i] * attenuation;

	  float ndotv = clamp(dot(nn, vv), 0.0, 1.0);
	  float ndotl = clamp(dot(nn, ld), 0.0, 1.0);
	  float ndoth = clamp(dot(nn, hh), 0.0, 1.0);
	  float hdotv = clamp(dot(hh, vv), 0.0, 1.0);

	  float d = distributionGGX(ndoth, inRoughness);
	  float g = geometrySmith(ndotv, ndotl, inRoughness);
	  vec3 f = calcFresnel(f0, hdotv, 1.0);

	  vec3 nominator = d * g * f;
	  // prevent divide by zero
	  float denominator = max(0.001, 4 * ndotv * ndotl); 
	  vec3 specular = nominator / denominator * ubSpecular;

	  // kS is equal to Fresnel
	  vec3 kS = f;
	  // for energy conservation, the diffuse and specular light can't
	  // be above 1.0 (unless the surface emits light); to preserve this
	  // relationship the diffuse component (kD) should equal 1.0 - kS.
	  vec3 kD = vec3(1.0) - kS;

	  // scale light by NdotL
	  vec3 diffuse = kD * albedo / pi * ubDiffuse;
	  direct += (diffuse + specular)*radiance*ndotl;
  }

  float ndotv = clamp(dot(nn, vv), 0.0, 1.0);
  vec3 envFresnel = calcFresnel(f0, ndotv, 1);
  vec3 r = 2.0 * nn * ndotv - vv; // =reflect(-toCamera, normal) 
  r = getSpecularDomninantDir(nn, r, inRoughness);
  vec3 kS = envFresnel;
  vec3 kD = 1.0 - envFresnel;
  vec3 irradiance  = texture(uEnvmapIrr, nn).xyz;

  // sample both the pre-filter map and the BRDF lut and combine them together as per the Split-Sum approximation to get the IBL specular part.
  const float MAX_REFLECTION_LOD = 4.0;
  vec3 prefilteredColor = textureLod(uEnvmapPrefilter, r, inRoughness * MAX_REFLECTION_LOD).rgb;    
  vec2 brdf = texture(uEnvmapBrdfLUT, vec2(ndotv, inRoughness)).rg;
  vec3 radiance = prefilteredColor * (kS * brdf.x + brdf.y);
  vec3 envDiffuse  = albedo*kD  * irradiance * ubDiffuseIbl;
  vec3 envSpecular = radiance   * ubSpecularIbl;
  vec3 indirect    = envDiffuse + envSpecular;

  // Color.
  vec3 color = direct + indirect;
  color = color * exp2(uExposure);
  // Is gamma correction is included?
  fragColor.xyz = toFilmic(color);
  fragColor.w = 1.0;
}

