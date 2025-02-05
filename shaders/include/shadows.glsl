struct CascadeSplit
{
    // used in array and array forces align(16)
    float nearPlane;
    float farPlane;
    vec2 pad;
};


float getViewSpaceDepth(vec3 worldPos, mat4 viewMatrix) {
    vec4 viewPos = viewMatrix * vec4(worldPos, 1.0);
    return abs(viewPos.z);
}

int selectCascadeLevel(float viewSpaceDepth, CascadeSplit[4] cascadeSplits) {
    if (viewSpaceDepth < cascadeSplits[0].farPlane) {
        return 0;
    }
    if (viewSpaceDepth < cascadeSplits[1].farPlane) {
        return 1;
    }
    if (viewSpaceDepth < cascadeSplits[2].farPlane) {
        return 2;
    }

    if (viewSpaceDepth < cascadeSplits[3].farPlane) {
        return 3;
    }

    return 4;
}

int getCascadeLevel(vec3 worldPos, mat4 viewMatrix, CascadeSplit cascadeSplits[4]) {
    float viewDepth = getViewSpaceDepth(worldPos, viewMatrix);
    return selectCascadeLevel(viewDepth, cascadeSplits);
}

float getShadowFactor(vec3 worldPos, mat4 cascadeLightViewProj, sampler2DShadow shadowMap, float cascadeNearPlane, float cascadeFarPlane) {
    vec4 lightSpacePos = cascadeLightViewProj * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    if (projCoords.z > cascadeFarPlane) { return 1.0; }
    if (projCoords.z < cascadeNearPlane) { return 1.0; }
    float shadow = texture(shadowMap, vec3(projCoords.xy, currentDepth));
    return shadow;
}

float getShadowFactorPCF5(vec3 worldPos, mat4 cascadeLightViewProj, sampler2DShadow shadowMap, float cascadeNearPlane, float cascadeFarPlane) {
    vec4 lightSpacePos = cascadeLightViewProj * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;
    if (projCoords.z > cascadeFarPlane) { return 1.0; }
    if (projCoords.z < cascadeNearPlane) { return 1.0; }

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);

    const int halfKernel = 2;
    for (int x = -halfKernel; x <= halfKernel; x++) {
        for (int y = -halfKernel; y <= halfKernel; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMap, vec3(projCoords.xy + offset, currentDepth));
        }
    }
    return shadow / ((2 * halfKernel + 1) * (2 * halfKernel + 1));
}