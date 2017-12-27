-- Vertex

// IN
layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec3 inNormal;

// OUT
out vec3 vNormalWS;
out vec3 vViewDirWS;
out vec3 vIrradiance;

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
  vViewDirWS = normalize(posWS - uEyePosWS);
}


--

//------------------------------------------------------------------------------


-- Fragment

// IN
in vec3 vNormalWS;
in vec3 vViewDirWS;

// OUT
layout(location = 0) out vec4 fColor;

// UNIFORM
uniform samplerCube uEnvmap;
uniform samplerCube uEnvmapIrr;

void main()
{  
  // Normal color for debugging
  fColor.rgb = 0.5f*vNormalWS + 0.5f;

  // change to get transparency
  fColor.a = 1.0f;
}

