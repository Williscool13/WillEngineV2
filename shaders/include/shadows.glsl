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
    vec4 comparisons = vec4(
    viewSpaceDepth <= cascadeSplits[0].farPlane,
    viewSpaceDepth <= cascadeSplits[1].farPlane,
    viewSpaceDepth <= cascadeSplits[2].farPlane,
    viewSpaceDepth <= cascadeSplits[3].farPlane
    );

    return 4 - int(dot(comparisons, vec4(1.0)));
}

int getCascadeLevel(vec3 worldPos, mat4 viewMatrix, CascadeSplit cascadeSplits[4]) {
    float viewDepth = getViewSpaceDepth(worldPos, viewMatrix);
    return selectCascadeLevel(viewDepth, cascadeSplits);
}

float getShadowFactorBlend(int pcfLevel, vec3 worldPos, mat4 sceneViewMatrix, CascadeSplit[4] splits, mat4[4] lightMatrices, sampler2DShadow[4] shadowMaps, float cascadeNearPlane, float cascadeFarPlane) {
    vec4 viewPos = sceneViewMatrix * vec4(worldPos, 1.0);
    float viewSpaceDepth = abs(viewPos.z);

    if (viewSpaceDepth < cascadeNearPlane) {
        // Too close = shadowed
        return 0.0f;
    }
    if (viewSpaceDepth > cascadeFarPlane) {
        // Too far = unshadowed
        return 1.0f;
    }

    int cascadeLevel = -1;
    vec4 comparisons = vec4(
    viewSpaceDepth <= splits[0].farPlane,
    viewSpaceDepth <= splits[1].farPlane,
    viewSpaceDepth <= splits[2].farPlane,
    viewSpaceDepth <= splits[3].farPlane
    );

    cascadeLevel = 4 - int(dot(comparisons, vec4(1.0)));

    if (cascadeLevel == -1) {
        // Shouldn't be legal
        return 1.0f;
    }


    vec4 lightSpacePos = lightMatrices[cascadeLevel] * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords.xy = projCoords.xy * 0.5 + 0.5;
    float currentDepth = projCoords.z;


    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMaps[nonuniformEXT(cascadeLevel)], 0);
    const int halfKernel = pcfLevel / 2;

    for (int x = -halfKernel; x <= halfKernel; x++) {
        for (int y = -halfKernel; y <= halfKernel; y++) {
            vec2 offset = vec2(x, y) * texelSize;
            shadow += texture(shadowMaps[nonuniformEXT(cascadeLevel)], vec3(projCoords.xy + offset, projCoords.z));
        }
    }
    float shadowFactor1 = shadow / ((2 * halfKernel + 1) * (2 * halfKernel + 1));

    if (cascadeLevel == 3) {
        float blendStart = splits[cascadeLevel].farPlane * 0.9;
        float blendEnd = splits[cascadeLevel].farPlane;
        if (viewSpaceDepth >= blendStart) {
            float blendFactor = smoothstep(blendStart, blendEnd, viewSpaceDepth);
            return mix(shadowFactor1, 1.0, blendFactor);
        }
    } else {
        float blendStart = splits[cascadeLevel + 1].nearPlane;
        float blendEnd = splits[cascadeLevel].farPlane;

        if (viewSpaceDepth >= blendStart) {
            vec4 nextLightSpacePos = lightMatrices[cascadeLevel + 1] * vec4(worldPos, 1.0);
            vec3 nextProjCoords = nextLightSpacePos.xyz / nextLightSpacePos.w;
            nextProjCoords.xy = nextProjCoords.xy * 0.5 + 0.5;

            shadow = 0.0;
            texelSize = 1.0 / textureSize(shadowMaps[nonuniformEXT(cascadeLevel + 1)], 0);

            for (int x = -halfKernel; x <= halfKernel; x++) {
                for (int y = -halfKernel; y <= halfKernel; y++) {
                    vec2 offset = vec2(x, y) * texelSize;
                    shadow += texture(shadowMaps[nonuniformEXT(cascadeLevel + 1)], vec3(nextProjCoords.xy + offset, nextProjCoords.z));
                }
            }
            float shadowFactor2 = shadow / ((2 * halfKernel + 1) * (2 * halfKernel + 1));

            float blendFactor = smoothstep(blendStart, blendEnd, viewSpaceDepth);

            return mix(shadowFactor1, shadowFactor2, blendFactor);
        }
    }
    return shadowFactor1;
}