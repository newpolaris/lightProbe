/*
 *          IblSkyBox.glsl
 *
 */

//------------------------------------------------------------------------------


-- Vertex

// IN
layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inTexcoord;

// OUT
out vec3 v_dir;

// UNIFORM
uniform vec4 uViewRect;
uniform mat4 uModelViewProjMatrix;
uniform mat4 uEnvViewMatrix;

void main()
{
	gl_Position = vec4(inPosition.xy, 1, 1);
	vec2 texCoord = inTexcoord;

	float fov = radians(45.0);
	float height = tan(fov*0.5);
	float aspect = height*(uViewRect.z / uViewRect.w);
	vec2 tex = (2.0*texCoord - 1.0) * vec2(aspect, height);
	v_dir = (uEnvViewMatrix * vec4(tex, 1.0, 0.0)).xyz;
}


--

//------------------------------------------------------------------------------


-- Fragment

// IN
in vec3 v_dir;

// OUT
layout(location = 0) out vec4 fragColor;

// UNIFORM
uniform samplerCube uCubemap;
uniform samplerCube uCubemapIrr;

void main()
{  
	vec3 dir = normalize(v_dir);
	fragColor = texture( uCubemapIrr, dir);
}
