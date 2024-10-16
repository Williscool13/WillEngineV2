const float MAX_REFLECTION_LOD = 4.0;
const uint DIFF_IRRA_MIP_LEVEL = 5;
const bool FLIP_ENVIRONMENT_MAP_Y = true;


vec3 DiffuseIrradiance(samplerCube envDiffSpec, vec3 N)
{
    vec3 ENV_N = N;
    if (FLIP_ENVIRONMENT_MAP_Y) { ENV_N.y = -ENV_N.y; }
    return textureLod(envDiffSpec, ENV_N, DIFF_IRRA_MIP_LEVEL).rgb;
}

vec3 SpecularReflection(samplerCube envDiffSpec, sampler2D lut, vec3 V, vec3 N, float roughness, vec3 F) {
    vec3 R = reflect(-V, N);
    if (FLIP_ENVIRONMENT_MAP_Y) { R.y = -R.y; }
    // dont need to skip mip 5 because never goes above 4
    vec3 prefilteredColor = textureLod(envDiffSpec, R, roughness * MAX_REFLECTION_LOD).rgb;

    vec2 envBRDF = texture(lut, vec2(max(dot(N, V), 0.0f), roughness)).rg;

    return prefilteredColor * (F * envBRDF.x + envBRDF.y);
}