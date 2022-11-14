#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
    uint type;
} params;


layout (location = 0 ) out VS_OUT
{
    vec3 wNorm;
    vec2 texCoord;
    vec3 color;
} vOut;

out gl_PerVertex { vec4 gl_Position; };
void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    vec3 wPos     = (params.mModel * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(params.mModel))) * wNorm.xyz);

    switch (params.type) {
        case 0: // Room walls
            vOut.color = vec3(0.733f, 1.f, 0.596f); break;            
        case 1: // Teapot
            vOut.color = vec3(1.f, 0.596f, 0.651f); break;            
        case 2: // Box
            vOut.color = vec3(1.f, 0.424f, 0.424f); break;            
        case 3: // Cylinder
            vOut.color = vec3(0.596f, 0.957f, 1.f); break;            
        case 4: // L-shaped figure
            vOut.color = vec3(0.749f, 0.725f, 1.f); break;            
        case 5: // Sphere
            vOut.color = vec3(1.f, 0.788f, 0.424f); break;            
    }
    vOut.texCoord = vTexCoordAndTang.xy;
    gl_Position   = params.mProjView * vec4(wPos, 1.0);
}
