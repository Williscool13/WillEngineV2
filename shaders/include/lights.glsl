struct DirectionalLight {
    vec3 direction;
    float intensity;
    vec3 color;
    float pad;
};

struct PointLight {
    vec4 position;
    vec3 color;
    float radius;
    int shadowMapIndex;
    vec3 pad;
};