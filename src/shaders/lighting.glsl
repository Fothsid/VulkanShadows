///
/// Vulkan Shadows
/// Author: Fedor Vorobev
///
/// Lighting helper functions. Includes shadow map sampling code.
///
#ifndef LIGHTING_GLSL
#define LIGHTING_GLSL

vec4 sampleBaseColor(vec2 texCoord) {
    vec4 texel = vec4(1,1,1,1);

    if (material.baseColorTID != 0) {
        texel = sampleTexture2D(material.baseColorTID + textureBaseIndex - 1,
                                material.samplerID,
                                texCoord);
        if (ENABLE_ALPHA_TEST != 0) {
            if (texel.a < material.alphaCutoff)
                discard;
        }
    }
    return texel;
}

struct LightResult {
    vec3 ambient;
    vec3 diffuse;
};

float getShadowFactor(vec4 pos, Light light) {
    if (light.shadowMap == 0)
        return 0;

    vec3 dir = pos.xyz - light.position;

    // Getting the cubemap face-specific depth and applying perspective
    // transformation to it to match the value range stored in the shadow map.
    const float n = light.zNear;
    const float f = light.zFar;
    vec3  a = abs(dir);
    float z = max(a.x, max(a.y, a.z));
    float d = 0.5 * (1 + (f + n) / (f - n) - (2.0 * f * n) / (f - n) / z);

    return sampleTextureCubeShadow(light.shadowMap, ShadowSampler, vec4(dir, d));
}

LightResult calculateLighting(vec4 pos, vec3 normal) {
    LightResult r = LightResult(vec3(0,0,0), vec3(0,0,0));

    for (uint i = 0; i < lightCount; ++i) {
        // Simple Lambertian diffuse with linear falloff.
        Light light = lights.data[i];
        vec3  toLight = light.position - pos.xyz;
        float distanceToLight = length(toLight);
        float normalizedDistnace = min(1.0, distanceToLight / light.range);
        float falloff = (1.0 - normalizedDistnace);
        vec3  normalizedDirectionToLight = normalize(toLight);
        float diffuseFactor = max(0, dot(normal, normalizedDirectionToLight))
                            * falloff * light.intensity;
        if (USE_SHADOW_MAPS != 0) {
            diffuseFactor *= 1.0 - getShadowFactor(pos, light);
        }
        r.diffuse += light.diffuse * diffuseFactor;
        r.ambient += light.ambient * falloff;
    }
    r.ambient += material.ambient.xyz;
    r.diffuse *= material.diffuse.xyz;
    return r;
}

#endif