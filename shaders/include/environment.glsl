const float MAX_REFLECTION_LOD = 4.0;
const uint DIFF_IRRA_MIP_LEVEL = 5;
const bool FLIP_ENVIRONMENT_MAP_Y = true;

#include "pbr.glsl"

vec3 DiffuseIrradiance(samplerCube envDiffSpec, vec3 worldNormal)
{
    vec3 ENV_N = worldNormal;
    if (FLIP_ENVIRONMENT_MAP_Y) { ENV_N.y = -ENV_N.y; }
    return textureLod(envDiffSpec, ENV_N, DIFF_IRRA_MIP_LEVEL).rgb;
}

vec3 SpecularReflection(samplerCube envDiffSpec, sampler2D lut, vec3 viewV, vec3 viewN, mat4 invView, float roughness, vec3 F0) {
    vec3 R = reflect(-viewV, viewN);

    vec3 worldR = normalize((invView * vec4(R, 0.0)).xyz);

    if (FLIP_ENVIRONMENT_MAP_Y) { worldR.y = -worldR.y; }
    // dont need to skip mip 5 because never goes above 4
    vec3 prefilteredColor = textureLod(envDiffSpec, worldR, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 envBRDF = texture(lut, vec2(max(dot(viewN, viewV), 0.0f), roughness)).rg;

    vec3 F = F_SCHLICK(viewV, viewN, F0);
    return prefilteredColor * (F * envBRDF.x + envBRDF.y);
}