/*
 *          IblSkyBox.glsl
 *
 */

//------------------------------------------------------------------------------


-- Vertex

// IN
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;

// OUT
out vec3 vDirection;

// UNIFORM
uniform mat4 uViewMatrix;
uniform mat4 uProjMatrix;

void main()
{
  mat4 rotView = mat4(mat3(uViewMatrix));
  vec4 clipPos = uProjMatrix * rotView * inPosition;
  gl_Position = clipPos.xyww;

  vDirection = inPosition.xyz;
}


--

//------------------------------------------------------------------------------


-- Fragment

// IN
in vec3 vDirection;

// OUT
layout(location = 0) out vec4 fragColor;

// UNIFORM
uniform samplerCube uEnvmap;
uniform samplerCube uEnvmapIrr;
uniform float uBgType;
uniform float uExposure;

vec3 toLinear(vec3 _rgb)
{
	return pow(abs(_rgb), vec3(2.2) );
}

vec4 toLinear(vec4 _rgba)
{
	return vec4(toLinear(_rgba.xyz), _rgba.w);
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

void main()
{  
  vec3 dir = normalize(vDirection);
  fragColor = texture( uEnvmap, dir );

  vec4 color;

  if (uBgType == 7.0)
  {
	  color = toLinear(texture(uEnvmapIrr, dir));
  }
  else
  {
	  float lod = uBgType;
	  dir = fixCubeLookup(dir, lod, 256.0);
	  color = toLinear(textureLod(uEnvmap, dir, lod));
  }
  color *= exp2(uExposure);

  fragColor = toFilmic(color);
}

