#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UBO_ViewProj_Inv {
    mat4 invProj;
    mat4 invView;
} ubo_viewProj_inv;

layout(binding = 1) uniform UBO_ViewProj_Shadow {
    mat4 viewProj;
} ubo_viewProj_Shadow;

layout(binding = 2) uniform sampler2D gBufferColor;
layout(binding = 3) uniform sampler2D gBufferNormal;
layout(binding = 4) uniform sampler2D gBufferDepth;
layout(binding = 5) uniform sampler2D shadowMap;
layout(binding = 6) uniform sampler2D albedoMap;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec2 fragPos;
layout(location = 0) out vec4 outColor;

void main() {
    // Read from G-buffers
    vec3 albedoColor = texture(albedoMap, fragUV).xyz;
    vec3 gBuffer_Color = texture(gBufferColor, fragUV).xyz;
    vec3 gBuffer_Normal = texture(gBufferNormal, fragUV).xyz * 2 - 1;

    // Get world space position from fragment position and depth
    float depth = texture(gBufferDepth, fragUV).r;
    vec3 ndcPos = vec3(fragPos, depth);
    vec4 viewPos = ubo_viewProj_inv.invProj * vec4(ndcPos, 1.0);
    viewPos /= viewPos.w;
    vec3 worldPos = (ubo_viewProj_inv.invView * viewPos).xyz;
    //outColor = vec4(worldPos, 1); return;

    // Shadow mapping
    worldPos += normalize(vec3(20.0) - worldPos) * 0.01;
    vec4 pointShadow = ubo_viewProj_Shadow.viewProj * vec4(worldPos, 1.0);
    //pointShadow /= pointShadow.w;
    pointShadow.xy = pointShadow.xy * 0.5 + 0.5;
    //outColor = vec4(vec3(pointShadow.z), 1.0); return;
    
    float shadowColor = texture(shadowMap, pointShadow.xy).r;
    float shadow = step(pointShadow.z, shadowColor);
    shadow = max(shadow, 0.15);
    float lambert = clamp(dot(normalize(vec3(20.0)), gBuffer_Normal), 0.0, 1.0);
    outColor = vec4(gBuffer_Color * shadow * lambert, 1.0);
    //outColor = vec4(vec3(shadowColor), 1.0);
    //outColor = vec4(vec3(pointShadow), 1.0);
}
