#version 450
layout(location = 0) in vec3 fragPosition;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform samplerCube environmentMap;



void main()
{
    // sample environment map
	vec3 direction = normalize(fragPosition);
	direction.y = -direction.y;
	vec3 envColor = texture(environmentMap, direction).rgb;

	// HDR to sRGB
	envColor = envColor / (envColor + vec3(1.0));
	envColor = pow(envColor, vec3(1.0 / 2.2));

    outColor = vec4(envColor, 1.0);
}