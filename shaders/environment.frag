#version 450
layout(location = 0) in vec3 fragPosition;
layout(location = 1) in vec2 uv;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform samplerCube environmentMap;

void main()
{
	vec3 direction = normalize(fragPosition);
	direction.y = -direction.y;
	vec3 envColor = texture(environmentMap, direction).rgb;

    outColor = vec4(envColor, 1.0);
}