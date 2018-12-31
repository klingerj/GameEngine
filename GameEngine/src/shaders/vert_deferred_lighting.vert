#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform UBO_ViewProj {
    mat4 viewProj;
} ubo_viewProj;

layout (binding = 2) uniform UBODynamic_ModelMat {
	mat4 model;
} uboDynamicModelMatInstance;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec2 fragPos;

out gl_PerVertex {
    vec4 gl_Position;
};

void main() {
    gl_Position = vec4(inPosition, 1.0);
    fragUV = vec2(inUV.x, inUV.y);
    fragPos = inPosition.xy;
}
