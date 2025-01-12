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

    return 3;
}


float getShadowFactor(vec3 worldPos, int cascadeIndex, sampler2D shadowMaps[4], mat4[4] lightMatrices) {
    // When using this function, the shader needs to import nonuniformEXT
    // #extension GL_EXT_nonuniform_qualifier: enable
    vec4 lightSpacePos = lightMatrices[cascadeIndex] * vec4(worldPos, 1.0);
    vec3 projCoords = lightSpacePos.xyz / lightSpacePos.w;
    projCoords = projCoords * 0.5 + 0.5;

    float closestDepth = texture(shadowMaps[nonuniformEXT(cascadeIndex)], vec2(projCoords.xy)).r;
    float currentDepth = projCoords.z;

    // > because of reversed depth buffer
    return currentDepth > closestDepth ? 0.0 : 1.0;
}

//float getShadow(vec3 worldPos, CascadeSplit[4] cascadeSplits, mat4 viewMatrix, sampler2D shadowMaps[4], mat4[4] lightMatrices) {
//    float viewDepth = getViewSpaceDepth(worldPos, viewMatrix);
//    int cascadeLevel = selectCascadeLevel(viewDepth, cascadeSplits);
//    float shadow = getShadowFactor(worldPos, cascadeLevel, shadowMaps, lightMatrices);
//    return shadow;
//}