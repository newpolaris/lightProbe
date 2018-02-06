//------------------------------------------------------------------------------

-- Fragment

out vec4 FragColor;
in vec3 WorldPos;

uniform samplerCube environmentCube;

const float pi = 3.14159265359;

void main()
{		
	// The world vector acts as the normal of a tangent surface
    // from the origin, aligned to WorldPos. Given this normal, calculate all
    // incoming radiance of the environment. The result of this radiance
    // is the radiance of light coming from -Normal direction, which is what
    // we use in the PBR shader to sample irradiance.
    vec3 normal = normalize(WorldPos);

    // tangent space calculation from origin point
    vec3 up = abs(normal.y) < 0.9999 ? vec3(0.0, 1.0, 0.0) : vec3(1.0, 0.0, 0.0);
    vec3 right = normalize(cross(up, normal));
    up = cross(normal, right);

	float sampleDelta = 0.025;
	float nrSampleCount = 0.0;
	vec3 irradiance = vec3(0.0);

	for (float phi = 0.0; phi < 2.0*pi; phi += sampleDelta)
	{
		for (float theta = 0.0; theta < 0.5 * pi; theta += sampleDelta)
		{
			// spherical to cartesian (in tangent space)
			vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
			// tangent space to world
			vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal;
			irradiance += texture(environmentCube, sampleVec).rgb * cos(theta) * sin(theta);
			nrSampleCount++;

		}
	}
	irradiance = irradiance * pi / nrSampleCount;
	FragColor = vec4(irradiance, 1.0);
}
