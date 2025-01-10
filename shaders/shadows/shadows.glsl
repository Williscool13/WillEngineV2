float getViewSpaceDepth(vec3 worldPos, mat4 viewMatrix) {
    vec4 viewPos = viewMatrix * vec4(worldPos, 1.0);
    return -viewPos.z;
}

int selectCascadeLevel(float viewSpaceDepth, vec4 cascadeSplits) {
    // cascadeSplits.x = near split between 0-1
    // cascadeSplits.y = split between 1-2
    // cascadeSplits.z = split between 2-3
    // cascadeSplits.w = far split

    if(viewSpaceDepth < cascadeSplits.x) return 0;
    if(viewSpaceDepth < cascadeSplits.y) return 1;
    if(viewSpaceDepth < cascadeSplits.z) return 2;
    return 3; // Last cascade
}


float getShadowFactor(vec3 worldPos, int cascadeIndex, sampler2DArray shadowMap, mat4[4] lightMatrices) {
    // Transform position to light space for the selected cascade
    vec4 lightSpacePos = lightMatrices[cascadeIndex] * vec4(worldPos, 1.0);

    // Perform perspective divide
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;

    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;

    // Sample shadow map from the correct cascade layer
    float closestDepth = texture(shadowMap, vec3(projCoords.xy, float(cascadeIndex))).r;
    float currentDepth = projCoords.z;

    // Compare depths
    return currentDepth > closestDepth ? 0.0 : 1.0;
    // > because of reversed depth buffer
}

float getShadow(vec3 worldPos, vec4 cascadeSplits, mat4 viewMatrix, sampler2DArray shadowMap, mat4[4] lightMatrices){
    float viewDepth = getViewSpaceDepth(worldPos, viewMatrix);
    int cascadeLevel = selectCascadeLevel(viewDepth, cascadeSplits);
    float shadow = getShadowFactor(worldPos, cascadeLevel, shadowMap, lightMatrices);
    return shadow;
}