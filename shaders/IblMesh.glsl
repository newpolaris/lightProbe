-- Vertex

// IN
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;

// OUT
out vec3 vNormalWS;
out vec3 vViewDirWS;

// UNIFORM
uniform mat4 uModelViewProjMatrix;
uniform vec3 uEyePosWS;

void main()
{
  // Clip Space position
  gl_Position = uModelViewProjMatrix * inPosition;

  // World Space normal
  vNormalWS = normalize( inNormal );
  
  // World Space view direction from world space position
  vec3 posWS = vec3(inPosition);
  vViewDirWS = normalize(uEyePosWS - posWS);
}


--

//------------------------------------------------------------------------------


-- Fragment

// IN
in vec3 vNormalWS;
in vec3 vViewDirWS;

// OUT
layout(location = 0) out vec4 fragColor;

// UNIFORM
uniform samplerCube uEnvmap;
uniform samplerCube uEnvmapIrr;

uniform bool ubMetalOrSpec;
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
uniform vec3 uRgbSpec;

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

vec3 toFilmic(vec3 _rgb)
{
  _rgb = max(vec3(0.0), _rgb - 0.004);
  _rgb = (_rgb*(6.2*_rgb + 0.5) ) / (_rgb*(6.2*_rgb + 1.7) + 0.06);
  return _rgb;
}

vec4 toFilmic(vec4 _rgba)
{
  return vec4(toFilmic(_rgba.xyz), _rgba.w);
}

vec3 toAcesFilmic(vec3 _rgb)
{
  // Reference:
  // ACES Filmic Tone Mapping Curve
  // https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
  float aa = 2.51f;
  float bb = 0.03f;
  float cc = 2.43f;
  float dd = 0.59f;
  float ee = 0.14f;
  return clamp( (_rgb*(aa*_rgb + bb) )/(_rgb*(cc*_rgb + dd) + ee), 0, 1 );
}

vec4 toAcesFilmic(vec4 _rgba)
{
  return vec4(toAcesFilmic(_rgba.xyz), _rgba.w);
}

vec3 toLinear(vec3 _rgb)
{
	return pow(abs(_rgb), vec3(2.2) );
}

vec4 toLinear(vec4 _rgba)
{
	return vec4(toLinear(_rgba.xyz), _rgba.w);
}

vec3 toLinearAccurate(vec3 _rgb)
{
	vec3 lo = _rgb / 12.92;
	vec3 hi = pow( (_rgb + 0.055) / 1.055, vec3(2.4) );
	vec3 rgb = mix(hi, lo, vec3(lessThanEqual(_rgb, vec3(0.04045) ) ) );
	return rgb;
}

vec4 toLinearAccurate(vec4 _rgba)
{
	return vec4(toLinearAccurate(_rgba.xyz), _rgba.w);
}

float toGamma(float _r)
{
	return pow(abs(_r), 1.0/2.2);
}

vec3 toGamma(vec3 _rgb)
{
	return pow(abs(_rgb), vec3(1.0/2.2) );
}

vec4 toGamma(vec4 _rgba)
{
	return vec4(toGamma(_rgba.xyz), _rgba.w);
}

vec3 toGammaAccurate(vec3 _rgb)
{
	vec3 lo  = _rgb * 12.92;
	vec3 hi  = pow(abs(_rgb), vec3(1.0/2.4) ) * 1.055 - 0.055;
	vec3 rgb = mix(hi, lo, vec3(lessThanEqual(_rgb, vec3(0.0031308) ) ) );
	return rgb;
}

vec4 toGammaAccurate(vec4 _rgba)
{
	return vec4(toGammaAccurate(_rgba.xyz), _rgba.w);
}

void main()
{  
  // Light.
  vec3 ld = normalize(uLightDir);
  vec3 clight = uLightCol;

  // Input.
  vec3 nn = normalize(vNormalWS);
  vec3 vv = normalize(vViewDirWS);
  vec3 hh = normalize(vv + ld);

  float ndotv = clamp(dot(nn, vv), 0.0, 1.0);
  float ndotl = clamp(dot(nn, ld), 0.0, 1.0);
  float ndoth = clamp(dot(nn, hh), 0.0, 1.0);
  float hdotv = clamp(dot(hh, vv), 0.0, 1.0);

  // Material params.
  vec3  inAlbedo = uRgbDiff;
  float inReflectivity = uReflectivity;
  float inGloss = uGlossiness;

  // Reflection.
  vec3 refl;
  if (false == ubMetalOrSpec) // Metalness workflow.
  {
      refl = mix(vec3(0.04), inAlbedo, inReflectivity);
  }
  else // Specular workflow.
  {
      refl = uRgbSpec * vec3(inReflectivity);
  }
  vec3 albedo = inAlbedo * (1.0 - inReflectivity);
  vec3 dirFresnel = calcFresnel(refl, hdotv, inGloss);
  vec3 envFresnel = calcFresnel(refl, ndotv, inGloss);

  vec3 lambert = ubDiffuse  * calcLambert(albedo * (1.0 - dirFresnel), ndotl);
  vec3 blinn   = ubSpecular * calcBlinn(dirFresnel, ndoth, ndotl, specPwr(inGloss));
  vec3 direct  = (lambert + blinn)*clight;

  // Note: Environment textures are filtered with cmft: https://github.com/dariomanesku/cmft
  // Params used:
  // --excludeBase true //!< First level mip is not filtered.
  // --mipCount 7       //!< 7 mip levels are used in total, [256x256 .. 4x4]. Lower res mip maps should be avoided.
  // --glossScale 10    //!< Spec power scale. See: specPwr().
  // --glossBias 2      //!< Spec power bias. See: specPwr().
  // --edgeFixup warp   //!< This must be used on DirectX9. When fileted with 'warp', fixCubeLookup() should be used.
  float mip = 1.0 + 5.0*(1.0 - inGloss); // Use mip levels [1..6] for radiance.

  vec3 vr = 2.0*ndotv*nn - vv; // Same as: -reflect(vv, nn);
  vec3 cubeR = normalize(vr);
  vec3 cubeN = normalize(nn);
  cubeR = fixCubeLookup(cubeR, mip, 256.0);

  vec3 radiance    = toLinear(textureLod(uEnvmap, cubeR, mip).xyz);
  vec3 irradiance  = toLinear(texture(uEnvmapIrr, cubeN).xyz);
  vec3 envDiffuse  = albedo     * irradiance * ubDiffuseIbl;
  vec3 envSpecular = envFresnel * radiance   * ubSpecularIbl;
  vec3 indirect    = envDiffuse + envSpecular;

  // Color.
  vec3 color = direct + indirect;
  color = color * exp2(uExposure);
  fragColor.xyz = toFilmic(color);
  fragColor.w = 1.0;
}

